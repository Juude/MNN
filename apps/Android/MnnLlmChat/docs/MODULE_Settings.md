# Settings Modules (Packages: `mainsettings`, `modelsettings`)

## 1. Purpose

The MnnLlmChat application provides two levels of settings:
1.  **Main (Global) Settings:** Application-wide configurations that affect overall behavior. Managed by the `mainsettings` package.
2.  **Model Settings:** Configurations specific to a particular LLM that are typically adjusted within a chat session. Managed by the `modelsettings` package.

## 2. Main Settings (`mainsettings` package)

### 2.1. Key Classes and Components

*   **`MainSettingsActivity.kt`:**
    *   **Role:** An `AppCompatActivity` that hosts the `MainSettingsFragment`.
*   **`MainSettingsFragment.kt`:**
    *   **Role:** A `PreferenceFragmentCompat` that displays and manages application-level preferences.
    *   **Responsibilities:**
        *   Loads preferences from an XML resource (e.g., `res/xml/main_settings_prefs.xml`).
        *   Handles user interactions with preferences (e.g., toggling switches, selecting options).
        *   Likely uses `PreferenceUtils` or direct SharedPreferences access to persist settings.
*   **`MainSettings.kt`:**
    *   **Role:** An object or utility class providing static accessors or constants related to main settings.
    *   **Example Setting:** `isStopDownloadOnChatEnabled(context)` which likely reads from SharedPreferences via `PreferenceUtils`.
*   **`PreferenceUtils.kt` (in `utils` package):**
    *   **Role:** A general utility for reading and writing SharedPreferences. Used by both main settings and potentially other parts of the app.
    *   **Key Constants:** Defines keys for various preferences (e.g., `KEY_SHOW_PERFORMACE_METRICS`, `KEY_STOP_DOWNLOAD_ON_CHAT`).

### 2.2. Core Logic and Data Flow

1.  User accesses main settings (e.g., from `MainActivity`'s navigation drawer or an options menu).
2.  `MainSettingsActivity` is launched, which then displays `MainSettingsFragment`.
3.  `MainSettingsFragment` loads its preference items from the XML resource.
4.  When a user changes a setting:
    *   The `PreferenceFragmentCompat` handles the UI change.
    *   The change is typically persisted to SharedPreferences automatically by the preference components or manually via an `OnPreferenceChangeListener`.
5.  Other parts of the app (e.g., `MainActivity`, `ChatActivity`) read these settings using `PreferenceUtils` or `MainSettings` accessors to modify their behavior.

### 2.3. Example Global Settings

*   Show performance metrics in chat.
*   Stop all model downloads when starting a chat session.
*   Theme selection (if available).
*   Default behavior for new sessions.

## 3. Model Settings (`modelsettings` package)

### 3.1. Key Classes and Components

*   **`SettingsBottomSheetFragment.kt`:**
    *   **Role:** A `BottomSheetDialogFragment` that displays model-specific settings during an active chat session in `ChatActivity`.
    *   **Responsibilities:**
        *   Receives the current `LlmSession` to interact with.
        *   Displays various configuration options relevant to the current LLM (e.g., max new tokens, system prompt, temperature - though specific UI elements for these are not fully detailed in provided code snippets, `ModelConfig` implies their existence).
        *   Allows users to modify these settings.
        *   Updates the `LlmSession` with new settings via its public methods (e.g., `updateMaxNewTokens()`, `updateSystemPrompt()`).
*   **`ModelConfig.kt`:**
    *   **Role:** Data class representing the configuration for an LLM, likely loaded from the model's `config.json` and potentially overridden by user settings.
    *   **Properties:** May include `assistantPromptTemplate`, `backendType`, `maxNewTokens`, `systemPrompt`, temperature, top_k, top_p etc.
    *   **Functionality:** `loadConfig()` static method to load from a base `config.json` and merge with custom settings from a `custom_config.json` (user-defined settings).
*   **`ModelPreferences.kt` (in `utils` package):**
    *   **Role:** Utility for reading and writing SharedPreferences that are specific to a given `modelId`. This allows per-model persistence of settings like "Use MMAP", "Use OpenCL Backend".
    *   **Key Constants:** `KEY_USE_MMAP`, `KEY_BACKEND`.
*   **`SettingsItemView.kt`, `SettingsRowSlideSwitch.java`, `DropDownLineView.kt`:**
    *   **Role:** Custom views likely used within `SettingsBottomSheetFragment` or other setting screens to present different types of preference controls (e.g., sliders, switches with labels, dropdowns).

### 3.2. Core Logic and Data Flow

1.  User is in `ChatActivity` and opens model settings (e.g., from the options menu).
2.  `ChatActivity` creates and shows an instance of `SettingsBottomSheetFragment`, passing the current `LlmSession` to it.
3.  `SettingsBottomSheetFragment` (or its views):
    *   May load current effective settings from `LlmSession.loadConfig()` or directly from `ModelPreferences` for things like MMAP/Backend.
    *   Displays these settings to the user.
4.  User changes a setting in the bottom sheet:
    *   For backend-dependent settings like max tokens, system prompt:
        *   The fragment calls corresponding methods on the `LlmSession` instance (e.g., `llmSession.updateMaxNewTokens(newValue)`).
        *   `LlmSession` then calls its native methods (e.g., `updateMaxNewTokensNative()`) to update the MNN engine's runtime parameters.
    *   For settings like "Use MMAP" or "Use OpenCL Backend":
        *   The change is saved using `ModelPreferences.setBoolean(context, modelId, key, value)`.
        *   `ChatActivity` typically needs to be recreated (`recreate()`) for these settings to take effect, as they influence the `LlmSession.load()` process.
5.  The `ModelConfig` object is central to how settings are applied during `LlmSession.load()`. It merges defaults from the model's directory with user-saved overrides.

### 3.3. Example Model-Specific Settings

*   Use MMAP for model loading.
*   Use OpenCL backend (vs CPU).
*   Max new tokens (generation length).
*   System prompt.
*   Assistant prompt template.
*   (Potentially) Temperature, Top-P, Top-K for sampling.

## 4. Dependencies

*   **AndroidX Preference Library:** For `PreferenceFragmentCompat` in `mainsettings`.
*   **AndroidX Core & AppCompat:** Standard UI components.
*   **Material Components:** For `BottomSheetDialogFragment`.
*   **`llm` Package:** `SettingsBottomSheetFragment` directly interacts with `LlmSession`.
*   **`utils` Package:** `PreferenceUtils` and `ModelPreferences` are heavily used for persistence.
*   **Gson:** Used by `ModelConfig` if it involves JSON parsing for configurations not directly from the primary `config.json`.

These settings modules provide crucial customization for both the overall app experience and the fine-tuning of individual model interactions.
