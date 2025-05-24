# Refactoring TODO List

This document outlines potential areas for refactoring in the MnnLlmChat Android application to improve code quality, maintainability, testability, and robustness.

## 1. Dependency Injection and Testability

*   **Issue:** Several key components directly instantiate their dependencies or rely on static singletons, making unit testing difficult.
    *   `ModelListPresenter` creates `HfApiClient` and `Handler` internally.
    *   `ModelDownloadManager` is a singleton accessed via `getInstance()`.
    *   `ChatPresenter` creates `ChatService` (via `provide()`) and `ScheduledExecutorService` internally.
    *   `LlmSession` (and similar) might directly use `ApplicationProvider.get()` for context.
*   **TODO:**
    *   **Introduce Dependency Injection (DI):** Consider using a DI framework like Hilt or Koin, or manual constructor/method injection.
        *   Inject `Context` where needed instead of relying on global providers if possible (especially in non-Android components if any).
        *   Inject dependencies like `HfApiClient`, `ModelDownloadManager` (or an interface for it), `ChatService`, `Handler` (or `CoroutineDispatcher`s) into presenters and sessions. This will simplify mocking in unit tests.
    *   **Refactor Singletons:** If `ModelDownloadManager` cannot be easily refactored out of its singleton pattern, ensure its static `getInstance()` can be overridden or configured for tests (e.g., by allowing a test instance to be set).
    *   **Abstract `Handler` usage:** Replace direct `Handler` usage with `CoroutineDispatcher` (e.g., `Dispatchers.Main`) where appropriate, or wrap Handler interactions in a testable way.

## 2. Static Utils and `ModelUtils.kt`

*   **Issue:** `ModelUtils.kt` contains many static helper functions. While useful, if these functions have complex logic or dependencies (e.g., on Android `Context` indirectly), they can be hard to test in isolation or mock for dependent classes. The use of `ModelUtils.isMultiModalModel(ModelItem)` was one such case where testing the adapter required assuming `ModelUtils` worked correctly.
*   **TODO:**
    *   **Review `ModelUtils.kt`:** Identify functions with complex logic or hidden dependencies.
    *   **Consider making some utilities instance-based if they rely on context or other state, and inject them.**
    *   **Add dedicated unit tests for complex static utility functions in `ModelUtils.kt`.** If they depend on Android APIs, they might need Robolectric or to be refactored to remove direct Android dependencies.

## 3. Presenter Logic and Responsibilities

*   **Issue:**
    *   `ModelListPresenter` handles API calls, caching, download listening, and UI updates. Some of this could be delegated to separate use case/interactor classes or repositories.
    *   `ChatPresenter` manages session creation, data management via `ChatDataManager`, and LLM request logic. Similarly, some data operations could be moved to a repository pattern.
*   **TODO:**
    *   **Introduce a Repository Pattern:** For data operations like fetching model lists (remote/cache), managing chat history (`ChatDataManager`), and handling model downloads. Presenters would then depend on these repositories. This can improve separation of concerns and testability.
    *   **Consider Use Cases/Interactors:** For more complex operations (e.g., the logic flow in `ChatPresenter.requestGenerate`), abstracting them into use case classes could make the presenter cleaner.

## 4. Asynchronous Operations and Coroutines

*   **Issue:** While Coroutines are used, consistency in error handling, cancellation, and structured concurrency could be reviewed. `ChatPresenter` uses both `presenterScope` and `GlobalScope` (implicitly via `ChatService` or other calls if not careful). `LlmSession` has manual `synchronized` blocks and `wait/notify` for managing `generating`/`modelLoading` flags with `releaseRequested`.
*   **TODO:**
    *   **Standardize Coroutine Scopes:** Ensure CoroutineScopes are tied to appropriate lifecycles (e.g., `viewModelScope`, `lifecycleScope` in Fragments/Activities, custom scopes for presenters that are explicitly cancelled). Avoid `GlobalScope` for tasks tied to component lifecycles.
    *   **Review `LlmSession` Concurrency:** Evaluate if the manual `synchronized` and `wait/notify` logic can be simplified or made more robust using Kotlin Coroutine primitives like `Mutex`, `CompletableDeferred`, or structured concurrency patterns. Ensure native calls within synchronized blocks are handled carefully to prevent deadlocks if native code also has locking.
    *   **Consistent Error Handling:** Ensure network calls and other fallible operations have robust and consistent error handling that propagates to the UI appropriately.

## 5. File and Cache Management

*   **Issue:** File paths for caching (model list, MMAP, model configs) are constructed in various places (e.g., `ModelListPresenter`, `FileUtils`, `LlmSession`).
*   **TODO:**
    *   **Centralize Path Management:** Create a dedicated `PathProvider` or similar utility, possibly injected, to be the single source of truth for all cache and file storage paths. This makes it easier to manage and change storage locations.
    *   **Review Cache Eviction/Management:** Currently, `saveToCache` in `ModelListPresenter` overwrites the model list. Consider if more sophisticated cache management (e.g., versioning, TTL, size limits) is needed for model files or other caches.

## 6. UI Components and XML Layouts

*   **Issue:** Some UI components like `ChatInputComponent` and `ChatListComponent` are custom and encapsulate significant logic. While this is good for modularity, their internal complexity might grow.
*   **TODO:**
    *   **Review Custom Component Logic:** Ensure these components remain focused on UI presentation and delegate complex business logic to presenters or view models.
    *   **Optimize Layouts:** Review complex layouts for performance, especially in `RecyclerView` item views. (This is a general Android best practice).

## 7. Native Code Interaction (JNI)

*   **Issue:** `LlmSession` directly manages `nativePtr` and JNI calls. Error handling from native calls, thread safety, and resource management are critical.
*   **TODO:**
    *   **Robust JNI Error Handling:** Ensure that any errors or exceptions from the native layer are caught and handled gracefully in the Kotlin bridge (e.g., converting them to Kotlin exceptions, logging detailed error info).
    *   **Thread Safety for Native Calls:** Verify that native methods are called on appropriate threads if they are not thread-safe internally. The current `synchronized` blocks in `LlmSession` aim to address this for key operations.
    *   **Lifecycle Management of Native Resources:** Ensure `releaseNative` is always called when a session is no longer needed to prevent memory leaks from the native side. The `releaseRequested` flag and `wait/notify` logic in `LlmSession` is an attempt to handle this during ongoing operations.

## 8. Code Clarity and Consistency

*   **Issue:** General code style, naming conventions, and comment quality can always be reviewed.
*   **TODO:**
    *   **Enforce Consistent Code Style:** Use Kotlin standard conventions and potentially a linter/formatter.
    *   **Improve Comments and Documentation:** Add KDoc for public APIs and clarify complex logic sections.
    *   **Refactor Complex Methods:** Break down long or overly complex methods into smaller, more manageable private functions.

This list is a starting point and would benefit from more in-depth code review by developers familiar with the project's history and specific challenges.
