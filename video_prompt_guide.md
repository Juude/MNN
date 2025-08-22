
# How to Run a Video Prompt with MNN

This document explains how to run a video prompt using the SmolVLM model within the MNN project.

## Prerequisites

Before you can run a video prompt, you need to have the following prerequisites installed:

*   **Python 3.6+**
*   **PyTorch**
*   **TorchVision**
*   **Transformers**
*   **Num2Words**

You can install the required Python packages using pip:

```bash
pip install torch torchvision transformers num2words
```

You will also need to download a pretrained SmolVLM model from the Hugging Face Hub. For this guide, we will use a placeholder model name `HuggingFaceM4/SmolVLM-1.5B-Instruct`. Replace this with the actual model you intend to use.

## Running a Video Prompt

To run a video prompt, you will need to create a Python script that uses the `SmolVLMProcessor` and `SmolVLMForConditionalGeneration` classes from the `smolvlm` directory.

Here is an example script that shows how to load a video prompt, process it, and generate a response:

```python
import torch
from smolvlm.processing_smolvlm import SmolVLMProcessor
from smolvlm.modeling_smolvlm import SmolVLMForConditionalGeneration

# 1. Load the processor and model
processor = SmolVLMProcessor.from_pretrained("HuggingFaceM4/SmolVLM-1.5B-Instruct")
model = SmolVLMForConditionalGeneration.from_pretrained("HuggingFaceM4/SmolVLM-1.5B-Instruct", torch_dtype=torch.bfloat16, device_map="auto")

# 2. Load the prompt from the prompt_video file
with open("prompt_video", "r") as f:
    prompt = f.read()

# 3. Extract the video path and the question from the prompt
# The prompt format is: "question:<video>path/to/video.mp4</video>"
question, video_path = prompt.split("<video>")
video_path = video_path.split("</video>")[0]

# 4. Create the messages for the chat template
messages = [
    {
        "role": "user",
        "content": [
            {"type": "video", "path": video_path},
            {"type": "text", "text": question},
        ]
    }
]

# 5. Apply the chat template
inputs = processor.apply_chat_template(messages, add_generation_prompt=True)

# 6. Generate the response
generated_ids = model.generate(**inputs, max_new_tokens=256)
generated_texts = processor.batch_decode(generated_ids, skip_special_tokens=True)

print(generated_texts)

```

### How to Use the `prompt_video` File

The `prompt_video` file is a simple text file that contains the question and the path to the video. The format is:

```
what is in the video:<video>/path/to/your/video.mp4</video>
```

You can modify this file to change the question and the video you want to use. The provided Python script is designed to parse this format.

### Running the Script

1.  Save the code above as a Python file (e.g., `run_video_prompt.py`) in the root of the MNN project.
2.  Make sure the `smolvlm` directory is in the same directory as your script, or is in your Python path.
3.  Modify the `prompt_video` file to point to the video you want to analyze.
4.  Run the script from your terminal:

```bash
python run_video_prompt.py
```

The script will then load the video, process the prompt, and print the model's response to the console.
