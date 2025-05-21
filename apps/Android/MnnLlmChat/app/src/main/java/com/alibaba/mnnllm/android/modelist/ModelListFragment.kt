// Created by ruoyi.sjd on 2025/1/13.
// Copyright (c) 2024 Alibaba Group Holding Limited All rights reserved.
package com.alibaba.mnnllm.android.modelist

import android.os.Bundle
// Explicit imports as requested, though they were effectively present.
import com.alibaba.mnnllm.android.modelist.DownloadState
import com.alibaba.mnnllm.android.modelist.Modality
import android.util.Log
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.appcompat.widget.SearchView
import androidx.core.view.MenuHost
import androidx.core.view.MenuProvider
import androidx.fragment.app.Fragment
import androidx.lifecycle.Lifecycle
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.alibaba.mls.api.ModelItem
import com.alibaba.mnnllm.android.MainActivity
import com.alibaba.mnnllm.android.R
import com.alibaba.mnnllm.android.mainsettings.MainSettingsActivity
import com.alibaba.mnnllm.android.utils.CrashUtil
// import com.alibaba.mnnllm.android.utils.PreferenceUtils.isFilterDownloaded // Removed
// import com.alibaba.mnnllm.android.utils.PreferenceUtils.setFilterDownloaded // Removed
import com.alibaba.mnnllm.android.utils.RouterUtils.startActivity

class ModelListFragment : Fragment(), ModelListContract.View, FilterSelectionDialogFragment.FilterOptionListener {

    companion object {
        private const val REQUEST_CODE_DOWNLOAD_STATE = 1
        private const val REQUEST_CODE_MODALITY = 2
    }

    private lateinit var modelListRecyclerView: RecyclerView

    override var adapter: ModelListAdapter? = null
        private set
    private var modelListPresenter: ModelListPresenter? = null
    private val hfModelItemList: MutableList<ModelItem> = mutableListOf()

    private lateinit var modelListLoadingView: View
    private lateinit var modelListErrorView: View

    private var modelListErrorText: TextView? = null

    // Variables already named as per previous successful application.
    private var currentDownloadStateFilter: DownloadState = DownloadState.ALL
    private var currentModalityFilter: Modality = Modality.ALL
    private var filterQuery: String = ""

    private fun setupSearchView(menu: Menu) {
        val searchItem = menu.findItem(R.id.action_search)
        val searchView = searchItem.actionView as SearchView?
        if (searchView != null) {
            searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
                override fun onQueryTextSubmit(query: String): Boolean {
                    filterQuery = query
                    adapter!!.setFilter(filterQuery, currentDownloadStateFilter, currentModalityFilter)
                    return false
                }

                override fun onQueryTextChange(query: String): Boolean {
                    filterQuery = query
                    adapter!!.setFilter(filterQuery, currentDownloadStateFilter, currentModalityFilter)
                    return true
                }
            })
            searchItem.setOnActionExpandListener(object : MenuItem.OnActionExpandListener {
                override fun onMenuItemActionExpand(item: MenuItem): Boolean {
                    // SearchView is expanded
                    Log.d("SearchView", "SearchView expanded")
                    return true
                }

                override fun onMenuItemActionCollapse(item: MenuItem): Boolean {
                    // SearchView is collapsed
                    Log.d("SearchView", "SearchView collapsed")
                    filterQuery = "" // Clear the query
                    adapter?.setFilter(filterQuery, currentDownloadStateFilter, currentModalityFilter) // Re-apply
                    return true
                }
            })
        }
    }

    private val menuProvider: MenuProvider = object : MenuProvider {
        override fun onCreateMenu(menu: Menu, menuInflater: MenuInflater) {
            // Inflate your menu resource here
            menuInflater.inflate(R.menu.menu_main, menu)
            setupSearchView(menu)

            // Remove old submenu item logic - it's already gone as per file state.
            // New click listeners for ActionBar trigger items:
            val downloadStateFilterTrigger = menu.findItem(R.id.action_trigger_filter_download_state)
            downloadStateFilterTrigger.setOnMenuItemClickListener {
                val options = getDisplayNamesForDownloadState()
                val dialog = FilterSelectionDialogFragment.newInstance(
                    getString(R.string.filter_download_state),
                    options,
                    currentDownloadStateFilter.ordinal,
                    REQUEST_CODE_DOWNLOAD_STATE
                )
                dialog.setFilterOptionListener(this@ModelListFragment) // Set listener
                dialog.show(childFragmentManager, "DownloadStateFilterDialog")
                true
            }

            val modalityFilterTrigger = menu.findItem(R.id.action_trigger_filter_modality)
            modalityFilterTrigger.setOnMenuItemClickListener {
                val options = getDisplayNamesForModality()
                val dialog = FilterSelectionDialogFragment.newInstance(
                    getString(R.string.filter_modality),
                    options,
                    currentModalityFilter.ordinal,
                    REQUEST_CODE_MODALITY
                )
                dialog.setFilterOptionListener(this@ModelListFragment) // Set listener
                dialog.show(childFragmentManager, "ModalityFilterDialog")
                true
            }

            // --- Other Menu Items ---
            val issueMenu = menu.findItem(R.id.action_github_issue)
            issueMenu.setOnMenuItemClickListener { item: MenuItem? ->
                if (activity != null) {
                    (activity as MainActivity).onReportIssue(null)
                }
                true
            }

            // No longer need the old filterDownloadedMenu logic

            val settingsMenu = menu.findItem(R.id.action_settings)
            settingsMenu.setOnMenuItemClickListener {
                if (activity != null) {
                    startActivity(activity!!, MainSettingsActivity::class.java)
                }
                true
            }

            val starGithub = menu.findItem(R.id.action_star_project)
            starGithub.setOnMenuItemClickListener { item: MenuItem? ->
                if (activity != null) {
                    (activity as MainActivity).onStarProject(null)
                }
                true
            }
            val reportCrashMenu = menu.findItem(R.id.action_report_crash)
            reportCrashMenu.setOnMenuItemClickListener {
                if (CrashUtil.hasCrash()) {
                    CrashUtil.shareLatestCrash(context!!)
                }
                true
            }
        }

        override fun onMenuItemSelected(menuItem: MenuItem): Boolean {
            // This method will now only handle non-filter menu items if any are left to its logic.
            // Or can be removed if all items have specific listeners or are handled by system.
            // For this refactoring, filter items are handled by their setOnMenuItemClickListener.
            // Let's assume other items like settings, github, etc., are handled by their own
            // listeners set in onCreateMenu or default behavior.
            // So, returning false here allows other handlers to take over.
            return false
        }

        override fun onPrepareMenu(menu: Menu) {
            super<MenuProvider>.onPrepareMenu(menu)
            val menuResumeAllDownlods = menu.findItem(R.id.action_resume_all_downloads)
            menuResumeAllDownlods.setVisible(modelListPresenter!!.unfinishedDownloadCount > 0)
            menuResumeAllDownlods.setOnMenuItemClickListener { item: MenuItem? ->
                modelListPresenter!!.resumeAllDownloads()
                true
            }
            val reportCrashMenu = menu.findItem(R.id.action_report_crash)
            reportCrashMenu.isVisible = CrashUtil.hasCrash()

            val downloadFilterItem = menu.findItem(R.id.action_trigger_filter_download_state)
            if (currentDownloadStateFilter != DownloadState.ALL) {
                downloadFilterItem?.icon?.alpha = 130 // Dim if filter is active
            } else {
                downloadFilterItem?.icon?.alpha = 255 // Full opacity if filter is not active (ALL)
            }

            val modalityFilterItem = menu.findItem(R.id.action_trigger_filter_modality)
            if (currentModalityFilter != Modality.ALL) {
                modalityFilterItem?.icon?.alpha = 130 // Dim if filter is active
            } else {
                modalityFilterItem?.icon?.alpha = 255 // Full opacity if filter is not active (ALL)
            }
        }
    }

    // Implementation of FilterOptionListener
    override fun onOptionSelected(requestCode: Int, selectedOption: String, selectedIndex: Int) {
        when (requestCode) {
            REQUEST_CODE_DOWNLOAD_STATE -> {
                currentDownloadStateFilter = DownloadState.values()[selectedIndex]
            }
            REQUEST_CODE_MODALITY -> {
                currentModalityFilter = Modality.values()[selectedIndex]
            }
        }
        adapter?.setFilter(filterQuery, currentDownloadStateFilter, currentModalityFilter)
        activity?.invalidateOptionsMenu() // To trigger onPrepareMenu for icon updates
    }

    // Helper functions for display names
    private fun getDisplayNamesForDownloadState(): ArrayList<String> {
        return ArrayList(DownloadState.values().map {
            when (it) {
                DownloadState.ALL -> getString(R.string.filter_download_state_all)
                DownloadState.DOWNLOADING -> getString(R.string.filter_download_state_downloading)
                DownloadState.DOWNLOADED -> getString(R.string.filter_download_state_downloaded)
                DownloadState.NOT_DOWNLOADED -> getString(R.string.filter_download_state_not_downloaded)
            }
        })
    }

    private fun getDisplayNamesForModality(): ArrayList<String> {
        return ArrayList(Modality.values().map {
            when (it) {
                Modality.ALL -> getString(R.string.filter_modality_all)
                Modality.MULTIMODAL -> getString(R.string.filter_modality_multimodal)
                Modality.NON_MULTIMODAL -> getString(R.string.filter_modality_non_multimodal)
            }
        })
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_modellist, container, false)
        modelListRecyclerView = view.findViewById(R.id.model_list_recycler_view)
        modelListLoadingView = view.findViewById(R.id.model_list_loading_view)
        modelListErrorView = view.findViewById(R.id.model_list_failed_view)
        modelListErrorText = modelListErrorView.findViewById(R.id.tv_error_text)
        modelListErrorView.setOnClickListener {
            modelListPresenter!!.load()
        }
        modelListRecyclerView.setLayoutManager(
            LinearLayoutManager(
                context,
                LinearLayoutManager.VERTICAL,
                false
            )
        )
        adapter = ModelListAdapter(hfModelItemList)

        modelListRecyclerView.setAdapter(adapter)
        modelListPresenter = ModelListPresenter(requireContext(), this)
        adapter!!.setModelListListener(modelListPresenter)
        // filterDownloaded = isFilterDownloaded(context) // Removed
        // Initialize with current (default or persisted) filter states
        adapter!!.setFilter(filterQuery, currentDownloadStateFilter, currentModalityFilter)
        modelListPresenter!!.onCreate()
        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val menuHost: MenuHost = requireActivity()
        menuHost.addMenuProvider(menuProvider, viewLifecycleOwner, Lifecycle.State.RESUMED)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        modelListPresenter!!.onDestroy()
    }

    override fun onListAvailable() {
        modelListErrorView.visibility = View.GONE
        modelListLoadingView.visibility = View.GONE
        modelListRecyclerView.visibility = View.VISIBLE
    }

    override fun onLoading() {
        if (adapter!!.itemCount > 0) {
            return
        }
        modelListErrorView.visibility = View.GONE
        modelListLoadingView.visibility = View.VISIBLE
        modelListRecyclerView.visibility = View.GONE
    }

    override fun onListLoadError(error: String?) {
        if (adapter!!.itemCount > 0) {
            return
        }
        modelListErrorText!!.text = getString(R.string.loading_failed_click_tor_retry, error)
        modelListErrorView.visibility = View.VISIBLE
        modelListLoadingView.visibility = View.GONE
        modelListRecyclerView.visibility = View.GONE
    }

    override fun runModel(absolutePath: String?, modelId: String?) {
        (activity as MainActivity).runModel(absolutePath, modelId, null)
    }
}
