const path = require('path');
const mnn = require('../index.js'); // Use index.js entry point

console.log('MNN Version:', mnn.version());

const modelPath = path.join(__dirname, '../../benchmark/models/mobilenet-v1-1.0.mnn');
console.log('Loading model from:', modelPath);

try {
    const interpreter = mnn.Interpreter.createFromFile(modelPath);
    console.log('Interpreter created');

    const session = interpreter.createSession({numThread: 2});
    console.log('Session created');

    const inputTensor = interpreter.getSessionInput(session, null);
    if (inputTensor) {
        console.log('Input tensor found');
        console.log('Input shape:', inputTensor.shape());

        // Resize session if needed (e.g. for dynamic shapes, though mobilenet is usually fixed)
        interpreter.resizeSession(session);

        // Prepare input data
        // MobileNet V1 224x224 input
        const size = 1 * 3 * 224 * 224;
        const inputData = new Float32Array(size);
        // Fill with some dummy data
        for(let i=0; i<size; i++) {
            inputData[i] = Math.random();
        }

        // Create a host tensor with proper layout (implicitly handled by Tensor create) to copy data from
        // Note: For advanced usage we might need to specify layout/dimension type (Tensorflow/Caffe)
        // Here we create a default tensor (which is NCHW usually for MNN C++ API usage)
        const hostTensor = new mnn.Tensor(inputTensor.shape());
        hostTensor.setData(inputData);

        // Copy to session input
        inputTensor.copyFrom(hostTensor);
        console.log('Input data copied');
    } else {
        console.error('Input tensor not found');
    }

    const ret = interpreter.runSession(session);
    console.log('Session run result code:', ret);

    const outputTensor = interpreter.getSessionOutput(session, null);
    if (outputTensor) {
        console.log('Output tensor found');
        console.log('Output shape:', outputTensor.shape());

        // Create host tensor to read output
        const hostOutputTensor = new mnn.Tensor(outputTensor.shape());
        outputTensor.copyTo(hostOutputTensor);

        try {
            const outputData = hostOutputTensor.getData();
            console.log('Output data length:', outputData.length);
            console.log('First 5 output values:', outputData.slice(0, 5));
        } catch (err) {
            console.error('Failed to get output data:', err.message);
        }
    } else {
        console.error('Output tensor not found');
    }

} catch (e) {
    console.error('Error:', e);
}
