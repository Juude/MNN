# Supporting Video Input in SmolVLM

## 1. Goal

Enable the SmolVLM model within the MNN framework to process video inputs, in addition to its existing text and image capabilities. This will allow for multi-modal understanding involving video content. The initial reference for implementation will be the logic in `/Users/songjinde/git/MNNX/SmolVLM2-HighlightGenerator/transformersSmo/src/transformers/models/smolvlm`.

## 2. Architecture and Design

The overall approach is to treat a video as a sequence of image frames. We will extract frames from the video, process them through the existing visual pipeline, and then combine their representations in a way that captures temporal information before feeding them to the language model.

### 2.1 Video Processing Pipeline

The pipeline will consist of the following stages:

1.  **Video Decoding**: A video file (e.g., MP4) will be decoded into a sequence of raw image frames.
2.  **Frame Sampling**: To manage computational cost and focus on relevant parts of the video, we will sample frames at a specific rate (e.g., 1 frame per second).
3.  **Frame Feature Extraction**: Each sampled frame will be processed by a vision encoder, which appears to be `mVisionModule` in the current `omni.cpp`. The `smolvlmVisionProcess` logic seems to handle image processing. We will adapt this for video frames. A video is likely to be composed of multiple images.
4.  **Temporal Aggregation**: The features from individual frames need to be aggregated. A simple approach is concatenation. The reference implementation will guide the specific strategy for temporal modeling.
5.  **Input to LLM**: The final aggregated video features will be represented as a sequence of special tokens and embeddings, similar to how image data is handled.

### 2.2 Code Structure Changes

The implementation will be primarily in `transformers/llm/engine/src/omni.cpp` and `omni.hpp`.

-   **`Omni::multimodeProcess`**: This function will be extended to recognize a new "video" mode. It will parse the video file path and pass it to a new `videoProcess` function.
-   **`Omni::videoProcess(const std::string& file)` (New Function)**: This will be the main function for handling video data.
    -   It will use a video decoding library (e.g., OpenCV, which MNN might already use via `MNN::CV`) to open the video file and extract frames.
    -   It will iterate through the video, sample frames at a configurable rate.
    -   For each frame, it will call the vision processing logic (similar to `smolvlmVisionProcess`) to get embeddings.
    -   It will collect the embeddings and corresponding special tokens (`imgIds`).
-   **`Omni::tokenizer_encode`**: This function will be updated to recognize `<video>` tags, similar to how it handles `<img>` tags.
-   **Configuration**: The `LlmConfig` might need to be extended to include video-specific parameters like frame sampling rate.

## 3. Implementation Plan (Task Breakdown)

1.  **[Task 1: Project Setup]** Create this markdown file `docs/transformers/support_video.md` to document the design and plan. (Done)

2.  **[Task 2: Video Decoding]** Investigate MNN's capabilities for video decoding.
    -   If `MNN::CV` supports video reading (`cv::VideoCapture`-like functionality), use it.
    -   If not, we may need to add a dependency like `ffmpeg` or use system libraries.
    -   For now, we can assume a function `MNN::CV::VideoReader(file)` exists or can be created.

3.  **[Task 3: Implement `videoProcess`]**
    -   Create the function `Omni::videoProcess(const std::string& file)` in `omni.cpp` and declare it in `omni.hpp`.
    -   Inside this function:
        -   Open the video file.
        -   Define a sampling rate (e.g., 1 fps).
        -   Loop through the video, read frames, and sample them.
        -   For each sampled frame (which is a `VARP image`):
            -   Apply the `smolvlmVisionProcess` logic. It might need to be refactored into a helper function that can be called for each frame. Let's call it `process_one_frame`.
            -   The `smolvlmVisionProcess` takes an image and produces `imgIds` and pushes embeddings to `mVisionEmbeddings`. For video, we will accumulate these from all frames.
    -   The function should return a `std::vector<int>` containing all the generated token IDs for the entire video.

4.  **[Task 4: Integrate into Multi-modal pipeline]**
    -   In `Omni::multimodeProcess`, add a case for `mode == "video"` to call `videoProcess`.
    -   In `Omni::tokenizer_encode`, add logic to parse `<video>` tags and call `multimodeProcess`.

5.  **[Task 5: Refactor `smolvlmVisionProcess`]**
    -   The current `smolvlmVisionProcess` might be too monolithic. It handles both single global images and split large images.
    -   For video, we'll be processing many frames, which are likely of a standard size.
    -   We should refactor the core image-to-embedding logic into a helper function that `smolvlmVisionProcess` and `videoProcess` can both use.
    -   For instance, `smolvlmVisionProcess` could be refactored to handle the logic of how to process an image (split or not), and then call a `process_patch` function. The `videoProcess` would call `process_patch` for each frame. The reference implementation will be critical here.

6.  **[Task 6: Handling Temporal Aspects]**
    -   The reference implementation at `SmolVLM2-HighlightGenerator` needs to be analyzed to understand how it handles the sequence of frames.
    -   The simplest approach is just concatenating frame features. `videoProcess` will do this by repeatedly calling the frame processing logic, which appends to `mVisionEmbeddings` and `imgIds`.
    -   The reference may use special positional encodings or attention mechanisms for temporal relations. If so, we need to adapt `gen_position_ids` or the model architecture itself.

7.  **[Task 7: Testing]**
    -   Create a test case with a short video to verify the entire pipeline works end-to-end.
    -   This includes checking tokenization, video processing, embedding generation, and model inference. 