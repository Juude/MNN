const path = require('path');
const mnn = require(path.join(__dirname, '../build/Release/mnn-node.node'));

console.log('MNN Version:', mnn.version());
