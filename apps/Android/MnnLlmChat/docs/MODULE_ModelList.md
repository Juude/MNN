# Model List Module (`modelist` package)

## 1. Purpose

The Model List module is responsible for displaying available LLM and Diffusion models to the user, managing their download states, allowing users to filter and search for models, and initiating the process of running a selected model.

## 2. Key Classes and Components

*   **`ModelListFragment.kt`:**
    *   **Role:** The primary UI component for this module. It's a Fragment hosted by `MainActivity`.
    *   **Responsibilities:**
        *   Displays the list of models using a `RecyclerView`.
        *   Manages the ActionBar menu for this screen, including search and filter triggers.
        *   Handles user interactions for filtering (search query, download state, modality) by showing `FilterSelectionDialogFragment`.
        *   Communicates user actions (e.g., model click) to its presenter or the host Activity.
        *   Implements `ModelListContract.View` to receive updates from the presenter.
        *   Implements `FilterSelectionDialogFragment.FilterOptionListener` to handle selections from filter dialogs.
*   **`ModelListPresenter.kt`:**
    *   **Role:** Presenter for `ModelListFragment`, handling its business logic.
    *   **Responsibilities:**
        *   Fetches the list of models from a remote source (e.g., Hugging Face via `HfApiClient`) and/or local cache/assets.
        *   Manages `ModelDownloadManager` to get download information for each model and to initiate downloads.
        *   Implements `ModelItemListener` to handle clicks on model items (e.g., download, run).
        *   Implements `DownloadListener` to react to download progress, completion, or failure, and updates the UI accordingly via the `ModelListContract.View`.
        *   Handles caching of the model list.
        *   Provides data about unfinished downloads to the UI.
*   **`ModelListAdapter.kt`:**
    *   **Role:** RecyclerView Adapter for displaying `ModelItem` objects.
    *   **Responsibilities:**
        *   Binds `ModelItem` data and `ModelItemDownloadState` to `ModelItemHolder` views.
        *   Implements the core filtering logic based on search query, `DownloadState`, and `Modality`.
        *   Updates individual items or the whole list upon receiving new data or state changes from the presenter.
*   **`ModelItemHolder.kt`:**
    *   **Role:** ViewHolder for individual model items in the RecyclerView.
    *   **Responsibilities:**
        *   Displays model information (name, icon, download status, progress, tags).
        *   Handles click events on the item and its download button, delegating them to `ModelItemListener` (implemented by `ModelListPresenter`).
*   **`ModelListContract.kt`:**
    *   **Role:** Defines the contract between `ModelListFragment` (View) and `ModelListPresenter` (Presenter).
    *   **`View` Interface:** Methods for the presenter to call on the fragment (e.g., `onLoading`, `onListAvailable`, `onListLoadError`, `runModel`).
*   **`ModelItemDownloadState.kt`:**
    *   **Role:** Data class holding the download state information for a `ModelItem`, including `DownloadInfo`.
*   **`FilterState.kt`:**
    *   **Role:** Defines enums `DownloadState` and `Modality` used for filtering.
*   **`FilterSelectionDialogFragment.kt`:**
    *   **Role:** Reusable DialogFragment for allowing users to select filter options (e.g., specific download states or modalities).
*   **Supporting XML:**
    *   `fragment_modellist.xml`: Layout for `ModelListFragment`.
    *   `recycle_item_model.xml`: Layout for individual model items.
    *   `dialog_filter_options.xml`, `list_item_filter_option.xml`: Layouts for the filter selection dialog.
    *   `menu_main.xml`: Defines menu items for search and filter triggers in the ActionBar.

## 3. Core Logic and Data Flow

### 3.1. Model List Loading

1.  **`ModelListFragment` created:** `ModelListPresenter` is initialized.
2.  **`ModelListPresenter.onCreate()`:**
    *   Sets itself as the listener for `ModelDownloadManager`.
    *   Attempts to `loadFromCache()`. If successful, calls `onListAvailable()`.
    *   Calls `requestRepoList()` to fetch the latest list from the network.
3.  **`requestRepoList()`:**
    *   Calls `view.onLoading()`.
    *   Uses `HfApiClient` to search for model repositories. It tries a default host and a mirror.
    *   **On Success (from `HfApiClient`):**
        *   If it's the first successful response, sets this client as the `bestApiClient`.
        *   Calls `saveToCache()` with the fetched models.
        *   Calls `onListAvailable()` with the fetched models.
    *   **On Failure:** Increments error count. If all attempts fail, calls `view.onListLoadError()`.
4.  **`onListAvailable(models, onSuccessRunnable)` (internal in Presenter):**
    *   Processes the raw model list (e.g., `ModelUtils.processList()`).
    *   Gets current download states for all models using `getModelItemState()`.
    *   Calls `modelListAdapter.updateItems()` to refresh the UI.
    *   Executes `onSuccessRunnable` if provided.
    *   Calls `view.onListAvailable()` to hide loading indicators.

### 3.2. Filtering

1.  **User Interaction:**
    *   **Search:** User types in `SearchView` in `ModelListFragment`. `onQueryTextChange` or `onQueryTextSubmit` is triggered.
    *   **Filter Icon Click:** User clicks a filter icon (`action_trigger_filter_download_state` or `action_trigger_filter_modality`) in `ModelListFragment`.
2.  **`ModelListFragment`:**
    *   **Search:** Updates its `filterQuery` and calls `adapter.setFilter(query, currentState, currentModality)`.
    *   **Filter Icon:** Shows `FilterSelectionDialogFragment` with appropriate options.
3.  **`FilterSelectionDialogFragment`:**
    *   User selects an option.
    *   Dialog calls `FilterOptionListener.onOptionSelected()` (implemented by `ModelListFragment`).
4.  **`ModelListFragment.onOptionSelected()`:**
    *   Updates `currentDownloadStateFilter` or `currentModalityFilter`.
    *   Calls `adapter.setFilter(query, newState, newModality)`.
    *   Calls `activity?.invalidateOptionsMenu()` to update ActionBar icon states (e.g., alpha).
5.  **`ModelListAdapter.setFilter()`:**
    *   Stores the filter parameters.
    *   Calls its internal `filter()` method.
6.  **`ModelListAdapter.filter()` (internal):**
    *   Applies a chain of filters:
        1.  Download State filter (based on `currentDownloadState` and `modelItemDownloadStatesMap`).
        2.  Modality filter (based on `currentModality` and `ModelUtils.isMultiModalModel(item)`).
        3.  Search Query filter (checks model name and tags).
    *   Updates `filteredItems` and calls `notifyDataSetChanged()`.

### 3.3. Model Item Interaction (Click)

1.  User clicks on a model item in `ModelListFragment`'s RecyclerView.
2.  `ModelItemHolder` click listener calls `ModelItemListener.onItemClicked()` (implemented by `ModelListPresenter`).
3.  **`ModelListPresenter.onItemClicked(modelItem)`:**
    *   Debounces rapid clicks.
    *   **If `modelItem.isLocal`:** Calls `view.runModel(localPath, modelId)`.
    *   **Else (remote model):**
        *   Gets `DownloadInfo` from `ModelDownloadManager`.
        *   If `COMPLETED`: Gets downloaded path and calls `view.runModel()`.
        *   If `NOT_START`, `FAILED`, `PAUSED`: Calls `modelDownloadManager.startDownload(modelId)`.
        *   If `DOWNLOADING`: Shows a "please wait" Toast.

### 3.4. Download Management

*   `ModelDownloadManager` handles the actual download process.
*   `ModelListPresenter` acts as a `DownloadListener`:
    *   `onDownloadStart()`: Resets progress tracking for the model.
    *   `onDownloadProgress()`: Updates `ModelListAdapter` to show progress (throttled).
    *   `onDownloadFinished()`: Updates adapter, effectively changing state to "Downloaded".
    *   `onDownloadFailed()`: Updates adapter.
    *   `onDownloadPaused()`: Updates adapter.
    *   `onDownloadFileRemoved()`: Updates adapter.
*   All these callbacks typically post updates to the `ModelListAdapter` via the `mainHandler` in `ModelListPresenter` to ensure UI updates happen on the main thread.

## 4. Dependencies

*   **`MainActivity`:** Hosts the `ModelListFragment`.
*   **`ChatActivity`:** Launched by `MainActivity` when a model is run.
*   **`com.alibaba.mls.api`:** For `HfApiClient` (model list fetching) and `ModelDownloadManager` (downloads).
*   **`utils` package:** Particularly `ModelUtils` for model property checks.
*   **AndroidX libraries:** Fragment, RecyclerView, AppCompat, etc.

This module is central to the app's ability to provide selectable and downloadable models for the LLM interaction.
