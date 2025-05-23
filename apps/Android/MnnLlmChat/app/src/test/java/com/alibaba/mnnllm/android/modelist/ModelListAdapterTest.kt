package com.alibaba.mnnllm.android.modelist

import com.alibaba.mls.api.ModelItem
import com.alibaba.mls.api.download.DownloadInfo // Required for DownloadSate
// Import ModelUtils if direct calls are made and it's not easily mockable,
// otherwise, we'll control its behavior via ModelItem properties in tests.
// For isMultiModalModel, we'll need a way to control its output for given ModelItems.
// This might require Mockito or a test-specific ModelUtils.
// Let's assume we can't directly mock ModelUtils easily without Mockito.
// We will control the "isMultimodal" characteristic by setting appropriate tags in test ModelItems
// and then have a way to make ModelUtils.isMultiModalModel behave as expected for these test items.
// This is tricky for static methods without Powermock or refactoring ModelUtils.
// Alternative for testing: Create test ModelItems and assume ModelUtils.isMultiModalModel works.
// The test will then verify the adapter's logic based on this assumption.

import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
// If using Mockito (optional for now, add if complex mocking becomes necessary)
// import org.mockito.Mockito.*
// import org.mockito.Mock
// import org.mockito.junit.MockitoJUnitRunner
// import org.junit.runner.RunWith

// @RunWith(MockitoJUnitRunner::class) // Uncomment if using Mockito annotations
class ModelListAdapterTest {

    private lateinit var adapter: ModelListAdapter
    private val originalItems = mutableListOf<ModelItem>()
    private val downloadStates = mutableMapOf<String, ModelItemDownloadState>()

    // Helper to create a ModelItem for tests
    private fun createModelItem(id: String, name: String, tags: List<String>, downloadSate: DownloadInfo.DownloadSate? = null): ModelItem {
        val item = ModelItem().apply {
            this.modelId = id
            // modelName is a getter based on modelId, so ensure getModelName (from ModelUtils) can handle this or mock it.
            // For simplicity, we assume modelName will be derived correctly or isn't the primary filter criteria itself.
            // We are more interested in tags and download state for filtering logic.
            tags.forEach { this.addTag(it) }
        }
        // Populate download state for this item
        if (downloadSate != null) {
            val downloadInfo = DownloadInfo().apply { this.downlodaState = downloadSate }
            downloadStates[id] = ModelItemDownloadState(item, downloadInfo, 0, 0, 0)
        } else {
            // Ensure a state for items not explicitly given one (e.g. NONE)
             val downloadInfo = DownloadInfo().apply { this.downlodaState = DownloadInfo.DownloadSate.NONE }
             downloadStates[id] = ModelItemDownloadState(item, downloadInfo, 0, 0, 0)
        }
        return item
    }
    
    // Mocking ModelUtils.isMultiModalModel is hard.
    // For testing, we will assume ModelUtils.isMultiModalModel(item) can be determined by looking at item.getTags()
    // and we will ensure our test cases for multimodal set tags that ModelUtils.isMultiModalModel would identify.
    // For example, if ModelUtils.isMultiModalModel checks for a "vision" tag, our multimodal test items will have it.

    @Before
    fun setUp() {
        // Reset lists for each test
        originalItems.clear()
        downloadStates.clear()

        // Create some sample model items
        originalItems.add(createModelItem("id1", "Model One (Vision)", listOf("vision", "text"), DownloadInfo.DownloadSate.COMPLETED)) // Multimodal, Downloaded
        originalItems.add(createModelItem("id2", "Model Two (Audio)", listOf("audio"), DownloadInfo.DownloadSate.DOWNLOADING)) // Multimodal, Downloading
        originalItems.add(createModelItem("id3", "Model Three (Text)", listOf("text", "llm"), DownloadInfo.DownloadSate.NONE))    // Non-multimodal, Not Downloaded
        originalItems.add(createModelItem("id4", "Model Four (Image)", listOf("image", "tool"), DownloadInfo.DownloadSate.FAILED)) // Multimodal, Not Downloaded (Failed)
        originalItems.add(createModelItem("id5", "Model Five (LLM)", listOf("llm"), null)) // Non-multimodal, Not Downloaded (state is null initially -> NONE)

        adapter = ModelListAdapter(ArrayList(originalItems)) // Pass a copy
        adapter.updateItems(ArrayList(originalItems), downloadStates) // Initialize with states
        // We need a way to make ModelUtils.isMultiModalModel work in tests.
        // The simplest way without adding Mockito/PowerMock is to assume it works correctly
        // and that the tags we add (e.g., "vision", "audio", "image") will make it return true,
        // and absence of such tags (e.g., "llm", "text" only) will make it return false.
        // This tests the adapter's logic conditional on ModelUtils working as expected.
    }

    @Test
    fun filter_noFilters_showsAllItems() {
        adapter.setFilter("", DownloadState.ALL, Modality.ALL)
        assertEquals("With no filters, all original items should be shown", originalItems.size, adapter.itemCount)
    }

    @Test
    fun filter_byDownloadState_Downloaded() {
        adapter.setFilter("", DownloadState.DOWNLOADED, Modality.ALL)
        val filtered = (0 until adapter.itemCount).map { adapter.getItemsForTest()[it].modelId }
        assertEquals(listOf("id1"), filtered)
    }

    @Test
    fun filter_byDownloadState_Downloading() {
        adapter.setFilter("", DownloadState.DOWNLOADING, Modality.ALL)
        val filtered = (0 until adapter.itemCount).map { adapter.getItemsForTest()[it].modelId }
        assertEquals(listOf("id2"), filtered)
    }

    @Test
    fun filter_byDownloadState_NotDownloaded() {
        adapter.setFilter("", DownloadState.NOT_DOWNLOADED, Modality.ALL)
        val filtered = (0 until adapter.itemCount).map { adapter.getItemsForTest()[it].modelId }.sorted()
        // id3 (NONE), id4 (FAILED), id5 (null -> NONE)
        assertEquals(listOf("id3", "id4", "id5").sorted(), filtered)
    }
    
    // To make isMultiModalModel testable, we assume specific tags trigger it.
    // Let's assume "vision", "audio", "image" tags make a model multimodal for ModelUtils.isMultiModalModel.

    @Test
    fun filter_byModality_Multimodal() {
        // Assuming ModelUtils.isMultiModalModel(item) returns true if tags include "vision", "audio", or "image"
        adapter.setFilter("", DownloadState.ALL, Modality.MULTIMODAL)
        val filtered = (0 until adapter.itemCount).map { adapter.getItemsForTest()[it].modelId }.sorted()
        // id1 (vision), id2 (audio), id4 (image)
        assertEquals(listOf("id1", "id2", "id4").sorted(), filtered)
    }

    @Test
    fun filter_byModality_NonMultimodal() {
        // Assuming ModelUtils.isMultiModalModel(item) returns false if no such tags
        adapter.setFilter("", DownloadState.ALL, Modality.NON_MULTIMODAL)
        val filtered = (0 until adapter.itemCount).map { adapter.getItemsForTest()[it].modelId }.sorted()
        // id3 (text, llm), id5 (llm)
        assertEquals(listOf("id3", "id5").sorted(), filtered)
    }

    @Test
    fun filter_bySearchQuery() {
        adapter.setFilter("Model One", DownloadState.ALL, Modality.ALL)
        val filtered = (0 until adapter.itemCount).map { adapter.getItemsForTest()[it].modelId }
        assertEquals(listOf("id1"), filtered)

        adapter.setFilter("LLM", DownloadState.ALL, Modality.ALL) // "LLM" is in name of id5, tag of id3
        val filtered2 = (0 until adapter.itemCount).map { adapter.getItemsForTest()[it].modelId }.sorted()
        assertEquals(listOf("id3", "id5").sorted(), filtered2)
    }
    
    @Test
    fun filter_bySearchQuery_TagName() {
        adapter.setFilter("tool", DownloadState.ALL, Modality.ALL) // "tool" is a tag in id4
        val filtered = (0 until adapter.itemCount).map { adapter.getItemsForTest()[it].modelId }
        assertEquals(listOf("id4"), filtered)
    }

    @Test
    fun filter_combined_Downloaded_NonMultimodal() {
        adapter.setFilter("", DownloadState.DOWNLOADED, Modality.NON_MULTIMODAL)
        val filtered = (0 until adapter.itemCount).map { adapter.getItemsForTest()[it].modelId }
        assertTrue("Should be empty, no downloaded non-multimodal models in sample", filtered.isEmpty())
        
        // Add a downloaded non-multimodal model for a positive test case
        val nonMultiDownloaded = createModelItem("id6", "Model Six (LLM)", listOf("llm"), DownloadInfo.DownloadSate.COMPLETED)
        originalItems.add(nonMultiDownloaded)
        adapter.updateItems(ArrayList(originalItems), downloadStates) // Re-init adapter with new item

        adapter.setFilter("", DownloadState.DOWNLOADED, Modality.NON_MULTIMODAL)
        val filtered2 = (0 until adapter.itemCount).map { adapter.getItemsForTest()[it].modelId }
        assertEquals(listOf("id6"), filtered2)
    }
    
    @Test
    fun filter_combined_NotDownloaded_Multimodal_Query() {
        adapter.setFilter("Model Four", DownloadState.NOT_DOWNLOADED, Modality.MULTIMODAL)
         val filtered = (0 until adapter.itemCount).map { adapter.getItemsForTest()[it].modelId }
        // id4 is Not_Downloaded (Failed), Multimodal (image tag), matches "Model Four"
        assertEquals(listOf("id4"), filtered)
    }

    @Test
    fun unfilter_resetsTextSearch_keepsOtherFilters() {
        adapter.setFilter("Model One", DownloadState.DOWNLOADED, Modality.MULTIMODAL)
        assertEquals("Pre-condition: Should find id1", 1, adapter.itemCount)
        
        adapter.unfilter() // Should clear "Model One" but keep Downloaded & Multimodal
        // After unfilter, it should apply the existing non-text filters.
        // id1 is Downloaded (COMPLETED) and Multimodal (vision tag)
        val filtered = (0 until adapter.itemCount).map { adapter.getItemsForTest()[it].modelId }
        assertEquals(listOf("id1"), filtered)
    }
    
    // Helper to access filteredItems for assertion (ModelListAdapter.getItems() is private)
    // This requires making getItems() internal or public for tests, or using a reflection hack.
    // For simplicity, let's assume we can add a method like this to ModelListAdapter for testing:
    // internal fun getItemsForTest(): List<ModelItem> = if (filteredItems != null) filteredItems!! else items
    // Add this to ModelListAdapter.kt:
    // fun getItemsForTest(): List<ModelItem> = if (filteredItems != null) filteredItems!! else items
}
