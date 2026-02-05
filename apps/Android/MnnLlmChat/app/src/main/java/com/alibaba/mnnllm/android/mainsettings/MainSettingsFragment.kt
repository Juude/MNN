// Created by ruoyi.sjd on 2025/2/28.
// Copyright (c) 2024 Alibaba Group Holding Limited All rights reserved.

package com.alibaba.mnnllm.android.mainsettings

import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.fragment.app.Fragment
import androidx.preference.PreferenceManager
import com.alibaba.mls.api.source.ModelSources
import com.alibaba.mnnllm.android.MNN
import com.alibaba.mnnllm.android.R
import com.alibaba.mnnllm.android.databinding.FragmentMainSettingsBinding
import com.alibaba.mnnllm.android.debug.DebugActivity
import com.alibaba.mnnllm.android.update.UpdateChecker
import com.alibaba.mnnllm.android.utils.AppUtils
import com.alibaba.mnnllm.api.openai.manager.ApiServiceManager
import com.alibaba.mnnllm.api.openai.service.ApiServerConfig
import com.google.android.material.dialog.MaterialAlertDialogBuilder

class MainSettingsFragment : Fragment() {

    private var _binding: FragmentMainSettingsBinding? = null
    private val binding get() = _binding!!

    private var updateChecker: UpdateChecker? = null
    private var debugClickCount = 0
    private val debugClickHandler = Handler(Looper.getMainLooper())
    private var debugClickRunnable: Runnable? = null
    private var updateCheckRunnable: Runnable? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentMainSettingsBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        setupSettings()
    }

    override fun onResume() {
        super.onResume()
        updateChecker?.checkForUpdates(requireContext(), false)
    }

    private fun setupSettings() {
        val sharedPreferences = PreferenceManager.getDefaultSharedPreferences(requireContext())

        // Stop Download on Chat
        binding.itemStopDownload.isChecked = sharedPreferences.getBoolean("stop_download_on_chat", true)
        binding.itemStopDownload.setOnCheckedChangeListener { isChecked ->
            sharedPreferences.edit().putBoolean("stop_download_on_chat", isChecked).apply()
        }

        // Download Provider
        val providerList = listOf(ModelSources.sourceHuffingFace, ModelSources.sourceModelScope, "modelers")
        val currentProvider = MainSettings.getDownloadProviderString(requireContext())
        
        fun getProviderName(provider: String): String {
            return when (provider) {
                ModelSources.sourceHuffingFace -> provider
                ModelSources.sourceModelScope -> getString(R.string.modelscope)
                else -> getString(R.string.modelers)
            }
        }

        binding.dropdownDownloadProvider.setCurrentItem(currentProvider)
        binding.dropdownDownloadProvider.setDropDownItems(
            providerList,
            itemToString = { getProviderName(it as String) },
            onDropdownItemSelected = { _, item ->
                MainSettings.setDownloadProvider(requireContext(), item as String)
                Toast.makeText(requireContext(), R.string.settings_complete, Toast.LENGTH_LONG).show()
            }
        )

        // Voice Model Management
        binding.btnVoiceModelManagement.setOnClickListener {
            val voiceModelMarketBottomSheet = com.alibaba.mnnllm.android.chat.voice.VoiceModelMarketBottomSheet.newInstance()
            voiceModelMarketBottomSheet.show(childFragmentManager, "voice_model_market")
        }

        // Enable API Service
        binding.itemEnableApi.isChecked = MainSettings.isApiServiceEnabled(requireContext())
        binding.itemEnableApi.setOnCheckedChangeListener { isChecked ->
            sharedPreferences.edit().putBoolean("enable_api_service", isChecked).apply()
        }

        // Reset API Config
        binding.btnResetApi.setOnClickListener {
            MaterialAlertDialogBuilder(requireContext())
                .setTitle(R.string.reset_api_config)
                .setMessage(R.string.reset_api_config_confirm_message)
                .setPositiveButton(android.R.string.ok) { _, _ ->
                    ApiServerConfig.resetToDefault(requireContext())
                    
                    if (MainSettings.isApiServiceEnabled(requireContext()) && ApiServiceManager.isApiServiceRunning()) {
                        ApiServiceManager.stopApiService(requireContext())
                        ApiServiceManager.startApiService(requireContext())
                        Toast.makeText(requireContext(), getString(R.string.api_config_reset_service_restarted), Toast.LENGTH_LONG).show()
                    } else {
                        Toast.makeText(requireContext(), getString(R.string.api_config_reset_to_default), Toast.LENGTH_LONG).show()
                    }
                }
                .setNegativeButton(android.R.string.cancel, null)
                .show()
        }

        // Check Update
        binding.btnCheckUpdate.apply {
            val versionInfo = getString(R.string.check_for_update) + " (" + AppUtils.getAppVersionName(requireContext()) + ")"
            text = versionInfo
            setOnClickListener {
                handleDebugClick()
                updateCheckRunnable?.let { debugClickHandler.removeCallbacks(it) }
                updateCheckRunnable = Runnable {
                    updateChecker = UpdateChecker(requireContext())
                    updateChecker?.checkForUpdates(requireContext(), true)
                }
                debugClickHandler.postDelayed(updateCheckRunnable!!, 1000L)
            }
        }

        // MNN Version
        try {
            val version = MNN.getVersion()
            binding.tvMnnVersion.text = getString(R.string.mnn_version_summary, version)
        } catch (e: Exception) {
            binding.tvMnnVersion.text = "N/A"
        }

        // Debug Mode
        val isDebugModeActivated = sharedPreferences.getBoolean("debug_mode_activated", false)
        updateDebugModeVisibility(isDebugModeActivated)
        
        binding.btnDebugMode.setOnClickListener {
            val intent = Intent(requireContext(), DebugActivity::class.java)
            startActivity(intent)
        }
    }

    private fun handleDebugClick() {
        debugClickCount++
        
        debugClickRunnable?.let { debugClickHandler.removeCallbacks(it) }
        
        if (debugClickCount >= DEBUG_CLICK_COUNT) {
            updateCheckRunnable?.let { debugClickHandler.removeCallbacks(it) }
            updateDebugModeVisibility(true)
            PreferenceManager.getDefaultSharedPreferences(requireContext()).edit()
                .putBoolean("debug_mode_activated", true).apply()
            debugClickCount = 0
            Log.d(TAG, "Debug mode preference activated")
        } else {
            debugClickRunnable = Runnable {
                debugClickCount = 0
                Log.d(TAG, "Debug click count reset due to timeout")
            }
            debugClickHandler.postDelayed(debugClickRunnable!!, DEBUG_CLICK_TIMEOUT)
        }
    }

    private fun updateDebugModeVisibility(visible: Boolean) {
        binding.dividerDebug.visibility = if (visible) View.VISIBLE else View.GONE
        binding.btnDebugMode.visibility = if (visible) View.VISIBLE else View.GONE
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
        debugClickRunnable?.let { debugClickHandler.removeCallbacks(it) }
        updateCheckRunnable?.let { debugClickHandler.removeCallbacks(it) }
    }

    companion object {
        const val TAG = "MainSettingsFragment"
        private const val DEBUG_CLICK_COUNT = 5
        private const val DEBUG_CLICK_TIMEOUT = 3000L // 3 seconds
    }
}
