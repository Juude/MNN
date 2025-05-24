# Chat Engine Module (Packages: `chat`, `llm`)

## 1. Purpose

The Chat Engine module is the core of the MnnLlmChat application, enabling users to interact with various Large Language Models (LLMs) and Diffusion models. It encompasses the user interface for chatting, the logic for managing chat sessions, the bridge to the native MNN inference engine, and local data persistence for chat history.

This module can be thought of in two main parts:
1.  **Chat UI & Presentation (`chat` package):** Handles how the user sees and interacts with a chat.
2.  **LLM Session Management & Native Bridge (`llm` package):** Handles the actual communication and computation with the loaded MNN models.

## 2. Key Classes and Components

### 2.1. `chat` Package (UI & Presentation)

*   **`ChatActivity.kt`:**
    *   **Role:** The main screen for a single chat session with a selected model.
    *   **Responsibilities:**
        *   Hosts and coordinates various UI components (`ChatInputComponent`, `ChatListComponent`).
        *   Manages the lifecycle of `ChatPresenter`.
        *   Handles ActionBar/Toolbar, options menu (new chat, settings, etc.).
        *   Initiates model loading display and updates UI based on generation state.
        *   Launches `SettingsBottomSheetFragment` for model-specific configurations.
*   **`ChatPresenter.kt`:**
    *   **Role:** Presenter for `ChatActivity`, managing the logic of a chat session.
    *   **Responsibilities:**
        *   Creates and manages `ChatSession` (either `LlmSession` or `DiffusionSession`) via `ChatService`.
        *   Handles user message submissions: constructs prompts, calls `chatSession.generate()`.
        *   Processes responses from `ChatSession` using `GenerateResultProcessor` and updates `ChatActivity` to display them.
        *   Manages chat history persistence for the current session via `ChatDataManager`.
        *   Handles session reset, stopping generation, and enabling/disabling audio output.
        *   Uses Coroutines for background tasks (model loading, generation).
*   **`ChatInputComponent.kt` (within `chat.input`):**
    *   **Role:** A custom component managing the user input area at the bottom of `ChatActivity`.
    *   **Responsibilities:**
        *   Handles text input.
        *   Manages UI for thinking mode toggle, audio output toggle.
        *   Handles actions like send message, stop generating.
        *   (Potentially) Integrates with voice recording (`VoiceRecordingModule`) and attachment picking (`AttachmentPickerModule`).
*   **`ChatListComponent.kt` (within `chat.chatlist`):**
    *   **Role:** A custom component managing the display of chat messages in a RecyclerView.
    *   **Responsibilities:**
        *   Uses `ChatRecyclerViewAdapter` to display `ChatDataItem` objects.
        *   Handles scrolling, updating individual messages (e.g., streaming LLM responses), and displaying performance metrics.
        *   Manages different view types for user messages, assistant responses, images, etc.
*   **`ChatRecyclerViewAdapter.kt`, `ChatViewHolders.kt` (within `chat.chatlist`):**
    *   Standard RecyclerView adapter and ViewHolder pattern for chat messages.
*   **`GenerateResultProcessor.kt`:**
    *   **Role:** Processes the raw output string from the LLM, potentially extracting structured information or handling special tokens (e.g., "thinking" blocks).
    *   `R1GenerateResultProcessor` is a specific implementation.
*   **`chat.model` Package:**
    *   `ChatDataItem.kt`: Data class representing a single message item (user or assistant).
    *   `SessionItem.kt`: Data class representing a chat session (for history).
    *   `ChatDataManager.kt`: Singleton responsible for CRUD operations on chat data (messages, sessions) using `ChatDatabaseHelper` (SQLite).
    *   `ChatDatabaseHelper.kt`: SQLiteOpenHelper implementation for managing the chat database schema.
*   **`PromptUtils.kt`, `SessionUtils.kt`:** Helper utilities for chat-related string manipulation and session naming.

### 2.2. `llm` Package (Session Management & Native Bridge)

*   **`ChatSession.kt` (Interface):**
    *   **Role:** Defines the common contract for different types of chat sessions (e.g., text LLM, Diffusion).
    *   **Key Methods:** `load()`, `generate()`, `reset()`, `release()`, `setKeepHistory()`, `setEnableAudioOutput()`.
*   **`LlmSession.kt`:**
    *   **Role:** Concrete implementation of `ChatSession` for interacting with text-based LLMs via a JNI bridge to the native MNN engine.
    *   **Responsibilities:**
        *   Manages a `nativePtr` to the C++ MNN LLM instance.
        *   `initNative()`: Loads the model into the MNN engine, passes configuration (model path, history, mmap settings, backend type).
        *   `submitNative()`: Sends the user's prompt to the MNN engine and receives responses via `GenerateProgressListener`.
        *   `resetNative()`, `releaseNative()`: Manages the lifecycle of the native MNN instance.
        *   Handles JNI interaction for features like audio output (`AudioDataListener`), debug info, and updating model parameters (max tokens, prompts).
        *   Manages internal state (`modelLoading`, `generating`, `releaseRequested`) to handle concurrent calls and proper resource release.
        *   Loads the `mnnllmapp` native library.
*   **`DiffusionSession.kt`:**
    *   **Role:** (Assumed, based on `ModelUtils.isDiffusionModel` and `ChatService.createDiffusionSession`) Concrete implementation of `ChatSession` for Diffusion models, likely also using a JNI bridge.
    *   **Responsibilities:** Similar to `LlmSession` but tailored for image generation models (e.g., different parameters for `generate`, handling image output paths).
*   **`ChatService.kt`:**
    *   **Role:** Service locator or factory for creating `ChatSession` instances (`LlmSession`, `DiffusionSession`).
    *   Manages active sessions.
*   **`GenerateProgressListener.kt` (Interface):**
    *   **Role:** Callback interface used by native code to stream back generation progress (partial responses) to the Java/Kotlin layer (`LlmSession`/`DiffusionSession`).
*   **`AudioDataListener.kt` (Interface):**
    *   **Role:** Callback interface for receiving audio data from multimodal models capable of speech output.

## 3. Core Logic and Data Flow

### 3.1. Initiating a Chat

1.  `MainActivity` calls `runModel()` with model details.
2.  `ChatActivity` is launched.
3.  `ChatActivity.onCreate()`:
    *   Initializes `ChatPresenter` with model ID and name.
    *   `ChatPresenter.createSession()`:
        *   Uses `ChatService.provide()` to get a `ChatService` instance.
        *   Calls `chatService.createLlmSession()` or `createDiffusionSession()` based on model type. This involves:
            *   Retrieving chat history for the session ID via `ChatDataManager` if it exists.
            *   Creating an `LlmSession` (or `DiffusionSession`) instance, which involves JNI setup but not yet loading the model into MNN.
    *   `ChatPresenter.load()`:
        *   Launches a coroutine.
        *   Notifies `ChatActivity` that loading has started (`onLoadingChanged(true)`).
        *   Calls `chatSession.load()`.
            *   **`LlmSession.load()`**: Marshals configuration (paths, history, mmap, backend) and calls `initNative()` to initialize the MNN engine with the model. This is a potentially long-running native call.
        *   Notifies `ChatActivity` that loading is finished (`onLoadingChanged(false)`).

### 3.2. Sending a Message and Receiving a Response (LLM Example)

1.  User types a message in `ChatInputComponent` and taps send.
2.  `ChatInputComponent` calls its `setOnSendMessage` listener, handled by `ChatActivity`.
3.  `ChatActivity.handleSendMessage(userDataItem)`:
    *   Calls `chatPresenter.requestGenerate(userDataItem, listener)`.
4.  `ChatPresenter.requestGenerate()`:
    *   Stores the `userDataItem` via `ChatDataManager`.
    *   Notifies `ChatActivity` that generation is starting (`generateListener.onGenerateStart()`).
    *   Launches an async coroutine:
        *   Calls `chatSession.generate(prompt, params, progressListener)`.
            *   **`LlmSession.generate()`**: Calls `submitNative()` with the prompt and a `GenerateProgressListener` implementation.
            *   Native MNN engine processes the prompt.
            *   As the MNN engine generates tokens, it calls back to `GenerateProgressListener.onProgress(partialResponse)` repeatedly.
            *   **`LlmSession` (in `onProgress` callback):** Passes the `partialResponse` to `ChatPresenter`'s listener (`generateListener.onLlmGenerateProgress()`).
        *   The final result map (including benchmark info) is returned from `chatSession.generate()`.
    *   Notifies `ChatActivity` that generation is finished (`generateListener.onGenerateFinished()`).
5.  **`ChatActivity` (handling `GenerateListener` callbacks):**
    *   `onGenerateStart()`: Adds user message to `ChatListComponent`, shows loading indicator for assistant message.
    *   `onLlmGenerateProgress()`: Updates the assistant's message in `ChatListComponent` with the streamed `partialResponse` (using `GenerateResultProcessor` to format).
    *   `onGenerateFinished()`: Finalizes the assistant's message, hides loading, displays benchmark info.
6.  `ChatPresenter.saveResponseToDatabase()`: Saves the assistant's final `ChatDataItem`.

### 3.3. Diffusion Model Flow (High-Level)

*   Similar to LLM, but `DiffusionSession.generate()` would take parameters relevant to image generation (e.g., output path, seed).
*   `GenerateProgressListener` would likely provide progress percentage.
*   The final result would include the path to the generated image, which `ChatActivity` then displays (e.g., in `ChatDataItem.imageUri`).

## 4. Native Interaction (JNI)

*   The `llm` package (specifically `LlmSession`, `DiffusionSession`) is the primary bridge to the native C++ code that uses MNN.
*   Native methods are declared with `external fun`.
*   A shared library (`libmnnllmapp.so`) containing the JNI implementations and MNN core logic must be loaded (`System.loadLibrary("mnnllmapp")`).
*   Data is passed between Java/Kotlin and C++ (e.g., prompts, configuration strings, history lists). Callbacks (`GenerateProgressListener`, `AudioDataListener`) allow native code to call back into Java/Kotlin.

## 5. Dependencies

*   **MNN Native Library:** The core dependency for model inference.
*   **UI Layer (`MainActivity`, AndroidX libraries):** For displaying chat and integrating into the app.
*   **`modelist` module:** To get the selected model details to start a chat.
*   **`utils` package:** For various helper functions.
*   **`com.alibaba.mls.api` (ApplicationProvider):** Used by `LlmSession` to get application context for `ModelPreferences`.
*   **Gson:** For JSON serialization/deserialization (e.g., model configs).

This module forms the heart of the application's interactive capabilities.
