// Created by ruoyi.sjd on 2024/12/25.
// Copyright (c) 2024 Alibaba Group Holding Limited All rights reserved.
package com.alibaba.mnnllm.android.modelist

import android.text.TextUtils
import android.view.LayoutInflater
import android.view.ViewGroup
import com.alibaba.mnnllm.android.modelist.DownloadState
import com.alibaba.mnnllm.android.modelist.Modality
import androidx.recyclerview.widget.RecyclerView
import com.alibaba.mls.api.ModelItem
import com.alibaba.mls.api.download.DownloadInfo
import com.alibaba.mnnllm.android.R
import java.util.Locale
import java.util.stream.Collectors

class ModelListAdapter(private val items: MutableList<ModelItem>) :
    RecyclerView.Adapter<RecyclerView.ViewHolder>() {
    private var filteredItems: List<ModelItem>? = null
    private var modelListListener: ModelItemListener? = null
    private var modelItemDownloadStatesMap: Map<String, ModelItemDownloadState>? = null
    private val modelItemHolders: MutableSet<ModelItemHolder> = HashSet()
    private var filterQuery: String = ""
    private var currentDownloadState: DownloadState = DownloadState.ALL
    private var currentModality: Modality = Modality.ALL

    fun setModelListListener(modelListListener: ModelItemListener?) {
        this.modelListListener = modelListListener
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view =
            LayoutInflater.from(parent.context).inflate(R.layout.recycle_item_model, parent, false)
        val holder = ModelItemHolder(view, modelListListener!!)
        modelItemHolders.add(holder)
        return holder
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        (holder as ModelItemHolder).bind(
            getItems()[position],
            modelItemDownloadStatesMap!![getItems()[position].modelId]
        )
    }


    override fun getItemCount(): Int {
        return getItems().size
    }

    fun updateItems(
        hfModelItems: List<ModelItem>,
        modelItemDownloadStatesMap: Map<String, ModelItemDownloadState>?
    ) {
        this.modelItemDownloadStatesMap = modelItemDownloadStatesMap
        items.clear()
        items.addAll(hfModelItems)
        // Apply current filters to the new list
        filter(this.filterQuery, this.currentDownloadState, this.currentModality)
    }

    fun updateItem(modelId: String) {
        val currentItems = getItems()
        var position = -1
        for (i in currentItems.indices) {
            if (currentItems[i].modelId == modelId) {
                position = i
                break
            }
        }
        if (position >= 0) {
            notifyItemChanged(position)
        }
    }

    fun updateProgress(modelId: String?, downloadInfo: DownloadInfo) {
        for (modelItemHolder in modelItemHolders) {
            if (modelItemHolder.itemView.tag == null) {
                continue
            }
            val tempModelId = (modelItemHolder.itemView.tag as ModelItem).modelId
            if (TextUtils.equals(tempModelId, modelId)) {
                modelItemHolder.updateProgress(downloadInfo)
            }
        }
    }

    private fun getItems(): List<ModelItem> {
        return if (filteredItems != null) filteredItems!! else items
    }

    private fun filter(query: String, downloadState: DownloadState, modality: Modality) {
        val lowerCaseQuery = query.lowercase(Locale.getDefault())

        val result = items.stream()
            .filter { hfModelItem: ModelItem ->
                // Download State Filter
                val downloadMatch = when (downloadState) {
                    DownloadState.DOWNLOADING -> {
                        val modelState = modelItemDownloadStatesMap?.get(hfModelItem.modelId)
                        modelState?.downloadInfo?.downlodaState == DownloadInfo.DownloadSate.DOWNLOADING
                    }
                    DownloadState.DOWNLOADED -> {
                        val modelState = modelItemDownloadStatesMap?.get(hfModelItem.modelId)
                        modelState?.downloadInfo?.downlodaState == DownloadInfo.DownloadSate.COMPLETED
                    }
                    DownloadState.NOT_DOWNLOADED -> {
                        val modelState = modelItemDownloadStatesMap?.get(hfModelItem.modelId)
                        val state = modelState?.downloadInfo?.downlodaState
                        state == null || state == DownloadInfo.DownloadSate.NONE || state == DownloadInfo.DownloadSate.FAILED
                    }
                    DownloadState.ALL -> true
                }
                if (!downloadMatch) return@filter false

                // Modality Filter
                val modalityMatch = when (modality) {
                    Modality.MULTIMODAL -> hfModelItem.isMultimodal
                    Modality.NON_MULTIMODAL -> !hfModelItem.isMultimodal
                    Modality.ALL -> true
                }
                if (!modalityMatch) return@filter false

                // Search Query Filter
                if (lowerCaseQuery.isEmpty()) return@filter true
                val modelName = hfModelItem.modelName!!.lowercase(Locale.getDefault())
                modelName.contains(lowerCaseQuery) ||
                        hfModelItem.newTags.stream().anyMatch { tag: String ->
                            tag.lowercase(Locale.getDefault()).contains(lowerCaseQuery)
                        }
            }
            .collect(Collectors.toList())

        if (query.isEmpty() && downloadState == DownloadState.ALL && modality == Modality.ALL) {
            this.filteredItems = null
        } else {
            this.filteredItems = result
        }
        notifyDataSetChanged()
    }

    fun unfilter() {
        this.filterQuery = "" // Clear only the text query
        filter(this.filterQuery, this.currentDownloadState, this.currentModality) // Re-apply with current dropdown filters
    }

    fun setFilter(filterQuery: String, downloadState: DownloadState, modality: Modality) {
        this.filterQuery = filterQuery
        this.currentDownloadState = downloadState
        this.currentModality = modality
        filter(this.filterQuery, this.currentDownloadState, this.currentModality)
    }
}
