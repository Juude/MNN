// Created by ruoyi.sjd on 2025/3/12.
// Copyright (c) 2024 Alibaba Group Holding Limited All rights reserved.

package com.alibaba.mnnllm.android.debug

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.view.View
import android.widget.Button
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.alibaba.mnnllm.android.R
import com.alibaba.mnnllm.android.asr.AsrService
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class DebugActivity : AppCompatActivity() {

    companion object {
        const val TAG = "DebugActivity"
        private const val REQUEST_RECORD_AUDIO_PERMISSION = 1001
    }

    private lateinit var scrollView: ScrollView
    private lateinit var logTextView: TextView
    private lateinit var asrTestButton: Button
    private lateinit var clearLogButton: Button
    
    private var recognizeService: AsrService? = null
    private var isRecording = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_debug)
        
        initViews()
        setupClickListeners()
        log("Debug Activity started")
    }

    private fun initViews() {
        scrollView = findViewById(R.id.scrollView)
        logTextView = findViewById(R.id.logTextView)
        asrTestButton = findViewById(R.id.asrTestButton)
        clearLogButton = findViewById(R.id.clearLogButton)
    }

    private fun setupClickListeners() {
        asrTestButton.setOnClickListener {
            if (isRecording) {
                stopAsrTest()
            } else {
                startAsrTest()
            }
        }

        clearLogButton.setOnClickListener {
            clearLog()
        }
    }

    private fun startAsrTest() {
        if (checkRecordAudioPermission()) {
            CoroutineScope(Dispatchers.Main).launch {
                try {
                    log("Starting ASR test...")
                    val modelDir = "/data/local/tmp/asr_models" 
                    recognizeService = AsrService(this@DebugActivity, modelDir)
                    
                    withContext(Dispatchers.IO) {
                        recognizeService?.initRecognizer()
                    }
                    recognizeService?.onRecognizeText = { text ->
                        runOnUiThread {
                            log("ASR Result: $text")
                        }
                    }

                    recognizeService?.startRecord()
                    isRecording = true
                    asrTestButton.text = getString(R.string.stop_asr_test)
                    log("ASR test started successfully")
                    
                } catch (e: Exception) {
                    log("ASR test failed: ${e.message}")
                    Log.e(TAG, "ASR test failed", e)
                }
            }
        }
    }

    private fun stopAsrTest() {
        try {
            recognizeService?.stopRecord()
            recognizeService = null
            isRecording = false
            asrTestButton.text = getString(R.string.start_asr_test)
            log("ASR test stopped")
        } catch (e: Exception) {
            log("Error stopping ASR test: ${e.message}")
            Log.e(TAG, "Error stopping ASR test", e)
        }
    }

    private fun checkRecordAudioPermission(): Boolean {
        return when {
            ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.RECORD_AUDIO
            ) == PackageManager.PERMISSION_GRANTED -> {
                true
            }
            shouldShowRequestPermissionRationale(Manifest.permission.RECORD_AUDIO) -> {
                Toast.makeText(this, R.string.recording_permission_denied, Toast.LENGTH_LONG).show()
                false
            }
            else -> {
                ActivityCompat.requestPermissions(
                    this,
                    arrayOf(Manifest.permission.RECORD_AUDIO),
                    REQUEST_RECORD_AUDIO_PERMISSION
                )
                false
            }
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        
        when (requestCode) {
            REQUEST_RECORD_AUDIO_PERMISSION -> {
                if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    log("Record audio permission granted")
                    startAsrTest()
                } else {
                    log("Record audio permission denied")
                    Toast.makeText(this, R.string.recording_permission_denied, Toast.LENGTH_LONG).show()
                }
            }
        }
    }

    private fun log(message: String) {
        val timestamp = java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.getDefault()).format(java.util.Date())
        val logMessage = "[$timestamp] $message\n"
        
        runOnUiThread {
            logTextView.append(logMessage)
            // 自动滚动到底部
            scrollView.post {
                scrollView.fullScroll(View.FOCUS_DOWN)
            }
        }
        
        Log.d(TAG, message)
    }

    private fun clearLog() {
        logTextView.text = ""
        log("Log cleared")
    }

    override fun onDestroy() {
        super.onDestroy()
        stopAsrTest()
    }
} 