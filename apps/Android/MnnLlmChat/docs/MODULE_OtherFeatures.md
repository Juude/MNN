# Other Features (Update, Benchmark)

This document covers supplementary features within the MnnLlmChat application, such as application updates and performance benchmarking.

## 1. Application Update (`update` package)

### 1.1. Purpose

The update module is responsible for checking for new versions of the MnnLlmChat application and notifying the user, potentially guiding them through the update process.

### 1.2. Key Classes and Components

*   **`UpdateChecker.kt`:**
    *   **Role:** The primary class responsible for handling application update checks.
    *   **Responsibilities:**
        *   `checkForUpdates(context: Context, manualCheck: Boolean)`:
            *   Likely contacts a predefined server or a GitHub releases page to get information about the latest version of the app.
            *   Compares the latest available version with the currently installed version.
            *   If a newer version is available, it may show a dialog to the user prompting them to update.
            *   The `manualCheck` parameter might control whether the user sees "no update available" messages or if it's a silent background check.
    *   **Data Source:** The URL for checking updates is likely hardcoded or configurable. This could be a GitHub API endpoint for releases or a custom server endpoint.
    *   **UI Interaction:** Shows dialogs to inform the user about available updates and might provide a link to download the new APK or navigate to a release page.

### 1.3. Core Logic and Data Flow

1.  **Automatic Check:** `UpdateChecker.checkForUpdates(context, false)` is called during `MainActivity.onCreate()`. This is a background check.
2.  **Manual Check:** The user might trigger a check from a menu item (e.g., in `MainActivity` via `checkForUpdate()`), which calls `UpdateChecker.checkForUpdates(context, true)`.
3.  `UpdateChecker` makes a network request (e.g., using HttpURLConnection, OkHttp, or another HTTP client - details not explored) to the update source.
4.  It parses the response (e.g., JSON containing version name, version code, download URL, release notes).
5.  Compares the fetched version code/name with `BuildConfig.VERSION_CODE` / `BuildConfig.VERSION_NAME` of the current app.
6.  If an update is found:
    *   An AlertDialog or custom dialog is displayed, showing new version information and prompting the user to download.
    *   Clicking "Update" might open a browser to a download URL or initiate an APK download directly (if permissions allow and it's implemented).
7.  If no update is found and it was a `manualCheck`, a "You are up to date" message might be shown.

## 2. Performance Benchmark (`benchmark` package)

### 2.1. Purpose

The benchmark module provides tools to measure the performance of LLM inferences, such as prefill speed and decoding speed (tokens per second). This is primarily a developer/testing feature but can be exposed to users.

### 2.2. Key Classes and Components

*   **`BenchmarkModule.kt`:**
    *   **Role:** Manages the benchmarking process within `ChatActivity`.
    *   **Properties:** `enabled` (controls if the benchmark menu item is visible).
    *   **Methods:**
        *   `start(waitForLastCompleted: suspend () -> Unit, handleSendMessage: suspend (String) -> HashMap<String, Any>)`:
            *   Takes lambdas to ensure previous generation is complete and to send messages for benchmarking.
            *   Likely iterates through a predefined set of prompts or uses a standard prompt.
            *   For each prompt, it calls `handleSendMessage` and measures the time taken for the response, possibly extracting performance metrics from the `HashMap` returned by `ChatSession.generate()`.
            *   May display results in Logcat, a Toast, or by updating a UI element (though UI for this is not explicitly detailed).
    *   **Integration:** Instantiated in `ChatActivity`. The "Benchmark Test" menu item in `ChatActivity` triggers its `start()` method.
*   **`BenchmarkTest.kt`:**
    *   (Content not fully explored) Could be a data class for holding benchmark parameters or results, or a helper class for running specific test scenarios.
*   **`DatasetOptionsAdapter.kt`, `SelectDataSetFragment.kt`:**
    *   (Content not fully explored) These suggest a more advanced benchmarking setup where users might be able to select different datasets or prompt sets for benchmarking. This might be a separate feature from the simple in-chat benchmark triggered from the menu.

### 2.3. Core Logic and Data Flow (In-Chat Benchmark)

1.  User is in `ChatActivity` and selects "Benchmark Test" from the options menu.
2.  `ChatActivity.onOptionsItemSelected()` calls `benchmarkModule.start()`.
3.  `BenchmarkModule.start()`:
    *   Ensures any ongoing generation is completed using `waitForLastCompleted()`.
    *   Possibly resets the `ChatSession` via `chatSession.reset()` or `chatSession.setKeepHistory(false)`.
    *   Iteratively (or once, depending on implementation):
        *   Constructs a benchmark prompt.
        *   Calls `handleSendMessage(prompt)` (which is `ChatActivity.handleSendMessage`, leading to `ChatPresenter` and `ChatSession.generate()`).
        *   The `ChatSession.generate()` method, when run, returns a `HashMap` that includes performance metrics (e.g., prefill time, decode time, token count).
        *   `BenchmarkModule` captures these metrics.
    *   Results are logged or displayed. `ModelUtils.generateBenchMarkString()` is used to format these metrics for display in `ChatDataItem`.

## 3. Dependencies

*   **Update Module:**
    *   Android `Context` for version checking and dialogs.
    *   Network client for fetching update information.
    *   (Potentially) JSON parsing library if the update info is in JSON.
*   **Benchmark Module:**
    *   `ChatActivity` context and its methods for sending messages.
    *   `ChatSession` (indirectly) as it provides the performance data.
    *   `ModelUtils` for formatting benchmark strings.

These features, while not part of the core chat or model listing experience, add value for maintenance (updates) and performance analysis (benchmarking).
