# MNN LLM Server (mls) Documentation

This document provides instructions on how to build, configure, and run the MNN LLM server (`mls` executable), and how to interact with its API.

## 1. Building the Server

The `mls` executable, which includes the LLM server functionality, is built as part of the main MNN project.

**Prerequisites:**
- CMake (version 3.6 or higher)
- A C++11 compliant compiler (e.g., GCC, Clang, MSVC)
- Standard build tools (e.g., make, ninja)
- OpenSSL development libraries (for HTTPS capabilities in other parts of `mls`, though not strictly for basic server HTTP)

**Build Steps:**

1.  **Clone the MNN repository (if not already done).**
    ```bash
    git clone https://github.com/alibaba/MNN.git
    cd MNN
    ```

2.  **Create a build directory and navigate into it:**
    ```bash
    mkdir build
    cd build
    ```

3.  **Run CMake to configure the build.**
    To build the `mls` executable, you need to enable the LLM components and specifically the `mls` target.
    ```bash
    cmake .. -DMNN_BUILD_LLM=ON -DBUILD_MLS=ON -DMNN_BUILD_TOOLS=ON -DMNN_BUILD_CONVERTER=ON -DMNN_BUILD_TRAIN=ON
    ```
    *   `-DMNN_BUILD_LLM=ON`: Enables building of LLM-related libraries and tools.
    *   `-DBUILD_MLS=ON`: Specifically enables the compilation of the `mls` executable (which contains the server).
    *   Other flags like `-DMNN_BUILD_TOOLS=ON`, `-DMNN_BUILD_CONVERTER=ON`, `-DMNN_BUILD_TRAIN=ON` can be useful for a complete MNN build.
    *   Use `-DMNN_BUILD_SHARED_LIBS=OFF` if you encounter linking issues with shared libraries related to `libMNN.a` index errors, as this forces a static build which often resolves such problems.

4.  **Compile the `mls` target:**
    ```bash
    make mls -j$(nproc)
    ```
    *(Replace `$(nproc)` with the number of cores you want to use for compilation, e.g., `make mls -j8`)*

5.  **Locate the executable:**
    The `mls` executable will be located at `build/mls`.

## 2. Model Preparation & Configuration

The MNN LLM server requires MNN-formatted model files.

**Model Files Overview:**
A typical MNN LLM model consists of:
- `llm.mnn`: The main model structure.
- `llm.mnn.weight`: The model weights.
- `embeddings_bf16.bin` (optional, depending on the model): Embeddings file.
- `tokenizer.txt`: Tokenizer vocabulary and merge information.

**Option A: Using a Real Model (Recommended, but currently has limitations)**

To use a real LLM, you first need to convert it from its original format (e.g., GGUF, SafeTensors) to the MNN format.

-   **Conversion Scripts:** Scripts for conversion are located in the `transformers/llm/export/` directory of the MNN repository (e.g., `gguf2mnn.py`, `safetensors2mnn.py`).
-   **Example (GGUF to MNN):**
    ```bash
    cd transformers/llm/export/
    python gguf2mnn.py --gguf /path/to/your_model.gguf --mnn_dir /path/to/output_mnn_model_directory/
    ```
-   **Critical Prerequisite - Template Files:**
    -   The conversion scripts (like `gguf2mnn.py`) require a template JSON file named `llm.mnn.json` to be present in the output directory (`--mnn_dir`). This file defines the base MNN graph structure that the script populates.
    -   Additionally, a configuration file named `llm_config.json` (see details below) must also be present in the output directory for the script to complete and for the server to use later.
    -   **Current Limitation:** As of the last update, the required template `llm.mnn.json` file is **missing** from the MNN repository. This currently blocks the straightforward conversion of new models using these scripts. Users would need to obtain or create this template file independently.

**Option B: Using Dummy Models (For Server Startup Testing)**

If you want to test the server's startup mechanism or API interaction without a real model, you can use dummy (empty) model files. The server will attempt to load them and will likely crash (e.g., with a segmentation fault), but this can verify the server starts and the API endpoints are registered.

1.  Create empty model files in your desired server working directory (e.g., `/app/` if running from there, or a dedicated model directory):
    ```bash
    touch llm.mnn
    touch llm.mnn.weight
    # touch tokenizer.txt (also needed)
    # touch embeddings_bf16.bin (if your llm_config.json implies it's needed)
    ```

**Configuration File (`llm_config.json`)**

The `mls` server requires a configuration file, typically named `llm_config.json`, to know which model files to load and what settings to use. The server looks for this file in its current working directory by default, or you can specify its path using the `-c` command-line argument.

**Example `llm_config.json`:**
```json
{
    "llm_model": "llm.mnn",
    "llm_weight": "llm.mnn.weight",
    "tokenizer_file": "tokenizer.txt",
    "embedding_file": "embeddings_bf16.bin",
    "backend_type": "cpu",
    "thread_num": 4,
    "precision": "low",
    "memory": "low",
    "power": "low",
    "use_mmap": "false",
    "is_batch_quant": 0,
    "reuse_kv": false,
    "quant_kv": 0,
    "kvcache_limit": 0,
    "max_seq_len": 2048,
    "max_new_tokens": 2048,
    "temperature": 1.0,
    "top_p": 0.9,
    "use_template": true,
    "tmp_path": "tmp"
}
```
*   Adjust `llm_model`, `llm_weight`, `tokenizer_file`, and `embedding_file` to point to your actual or dummy files.
*   The `tokenizer_file` and `embedding_file` entries might be necessary even with dummy models to prevent file-not-found errors during startup, depending on the server's loading logic. Create empty dummy files for these as well if needed.

## 3. Running the Server

1.  Navigate to the directory containing your `mls` executable (e.g., `/app/build/` if your MNN root is `/app`) or ensure `mls` is in your PATH.
2.  Ensure your `llm_config.json` and model files are in a location accessible to the server. For simplicity, you can place them in the same directory where you run the `mls` command, or in a dedicated model directory and provide the full path to the config.

3.  **Command to start the server:**
    If `llm_config.json` is in the current directory:
    ```bash
    ./mls serve -c llm_config.json
    ```
    Or, with full paths (example):
    ```bash
    /path/to/mnn/build/mls serve -c /path/to/your/model_dir/llm_config.json
    ```
    *(Run in the background by appending `&` if desired)*

4.  The server will attempt to start and listen on `http://localhost:9090` by default.

5.  **Note on Dummy Models:** If you are using the dummy (empty) model files as described in Option B, the server will likely start, try to parse these empty files as MNN models, fail, and then exit, often with a segmentation fault. This means the server *ran*, but could not become fully operational.

## 4. API Usage (`/chat/completions`)

The MNN LLM server provides an OpenAI-compatible API endpoint for chat completions.

**Endpoint:** `POST /chat/completions`

**Headers:**
- `Content-Type: application/json`

**Request Body (Non-streaming):**
```json
{
  "model": "your-model-name", // Can be any string, server doesn't use it currently
  "messages": [
    {"role": "system", "content": "You are a helpful assistant."},
    {"role": "user", "content": "Hello!"}
  ],
  "stream": false
}
```

**Example cURL (Non-streaming):**
```bash
curl -X POST http://localhost:9090/chat/completions \
     -H "Content-Type: application/json" \
     -d '{
           "model": "your-model-name",
           "messages": [
             {"role": "user", "content": "Hello!"}
           ],
           "stream": false
         }'
```

**Request Body (Streaming):**
Set `"stream": true` in the JSON payload.
```json
{
  "model": "your-model-name",
  "messages": [
    {"role": "system", "content": "You are a helpful assistant."},
    {"role": "user", "content": "Tell me a long story about a brave robot."}
  ],
  "stream": true
}
```

**Example cURL (Streaming):**
The `-N` (no-buffering) flag is recommended for streaming with cURL.
```bash
curl -X POST http://localhost:9090/chat/completions \
     -H "Content-Type: application/json" \
     -N \
     -d '{
           "model": "your-model-name",
           "messages": [
             {"role": "user", "content": "Tell me a story."}
           ],
           "stream": true
         }'
```
The server will send Server-Sent Events (SSE) for streaming responses. Each event will be a JSON object.

**Other Endpoints:**
-   `/reset`: `POST` endpoint to reset the conversation history/state in the LLM engine.
    ```bash
    curl -X POST http://localhost:9090/reset
    ```

## 5. Troubleshooting/Notes

-   **Missing `llm.mnn.json`:** The most significant current limitation is the absence of the `llm.mnn.json` template file in the MNN repository. This file is required by the model conversion scripts (e.g., `gguf2mnn.py`). Without it, converting new GGUF or SafeTensors models to the MNN format is not straightforward using the provided scripts.
-   **Server Behavior with Dummy Models:** If you run the server with empty `llm.mnn` and `llm.mnn.weight` files, it will likely start but then crash with a segmentation fault when it attempts to load these invalid model files. This is because the files lack the necessary MNN model structure.
-   **Configuration:** Ensure your `llm_config.json` correctly points to the model, weight, and tokenizer files. The server may fail to start or behave unexpectedly if these paths are incorrect.
-   **Working Directory:** The server typically expects model files and `llm_config.json` (if not specified with `-c`) relative to its current working directory.

```
