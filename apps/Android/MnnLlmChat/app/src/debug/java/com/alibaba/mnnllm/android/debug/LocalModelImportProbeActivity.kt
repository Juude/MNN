package com.alibaba.mnnllm.android.debug

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.DocumentsContract
import android.view.View
import android.widget.Button
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.alibaba.mnnllm.android.R
import com.alibaba.mnnllm.android.llm.LlmService
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout

class LocalModelImportProbeActivity : AppCompatActivity() {

    companion object {
        const val ACTION = "com.alibaba.mnnllm.android.debug.ACTION_LOCAL_MODEL_IMPORT_PROBE"
    }

    private lateinit var statusTextView: TextView
    private lateinit var logTextView: TextView
    private lateinit var scrollView: ScrollView
    private lateinit var manageStorageStateTextView: TextView
    private lateinit var grantManageStorageButton: Button
    private lateinit var pickFolderButton: Button

    private val manageStorageLauncher =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
            refreshManageStorageUi()
            if (ManageExternalStorageAccessHelper.isGranted()) {
                appendLog(getString(R.string.local_model_import_probe_manage_storage_now_granted))
            } else {
                appendLog(getString(R.string.local_model_import_probe_manage_storage_still_missing))
            }
        }

    private val openTreeLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        if (result.resultCode != RESULT_OK) {
            appendLog("Folder selection cancelled")
            return@registerForActivityResult
        }
        val treeUri = result.data?.data
        if (treeUri == null) {
            appendLog("Folder selection returned no Uri")
            return@registerForActivityResult
        }
        persistReadPermission(treeUri, result.data?.flags ?: 0)
        val report = LocalModelImportPathResolver.inspect(this, treeUri)
        statusTextView.text = report.status.name
        logTextView.text = report.toMultilineString()
        scrollView.post { scrollView.fullScroll(android.view.View.FOCUS_DOWN) }
        if (report.status == LocalModelImportPathResolver.ProbeStatus.RESOLVED_AND_PRECHECK_PASSED &&
            report.resolvedPath != null
        ) {
            runRuntimeProbe(report.resolvedPath)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_local_model_import_probe)

        statusTextView = findViewById(R.id.probeStatusTextView)
        logTextView = findViewById(R.id.probeLogTextView)
        scrollView = findViewById(R.id.probeScrollView)
        manageStorageStateTextView = findViewById(R.id.manageStorageStateTextView)
        grantManageStorageButton = findViewById(R.id.grantManageStorageButton)
        pickFolderButton = findViewById(R.id.pickFolderButton)

        refreshManageStorageUi()

        grantManageStorageButton.setOnClickListener {
            requestManageStorageAccess()
        }
        pickFolderButton.setOnClickListener {
            if (!ManageExternalStorageAccessHelper.isGranted()) {
                statusTextView.text = "MANAGE_EXTERNAL_STORAGE_REQUIRED"
                appendLog(getString(R.string.local_model_import_probe_manage_storage_required))
                requestManageStorageAccess()
                return@setOnClickListener
            }
            launchFolderPicker()
        }
        findViewById<Button>(R.id.clearProbeLogButton).setOnClickListener {
            statusTextView.text = getString(R.string.local_model_import_probe_idle)
            logTextView.text = ""
        }
        findViewById<Button>(R.id.copyProbeLogButton).setOnClickListener {
            val clipboard = getSystemService(CLIPBOARD_SERVICE) as android.content.ClipboardManager
            clipboard.setPrimaryClip(
                android.content.ClipData.newPlainText("Local Model Import Probe", logTextView.text)
            )
            Toast.makeText(this, R.string.log_copied_to_clipboard, Toast.LENGTH_SHORT).show()
        }
    }

    override fun onResume() {
        super.onResume()
        refreshManageStorageUi()
    }

    private fun launchFolderPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).apply {
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
            addFlags(Intent.FLAG_GRANT_PREFIX_URI_PERMISSION)
            putExtra(DocumentsContract.EXTRA_PROMPT, getString(R.string.local_model_import_probe_pick_folder))
        }
        openTreeLauncher.launch(intent)
    }

    private fun persistReadPermission(treeUri: Uri, resultFlags: Int) {
        val persistableFlags = resultFlags and
            (Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        runCatching {
            if (persistableFlags != 0) {
                contentResolver.takePersistableUriPermission(treeUri, persistableFlags)
            }
        }.onFailure { error ->
            appendLog("takePersistableUriPermission failed: ${error.message}")
        }
    }

    private fun refreshManageStorageUi() {
        val granted = ManageExternalStorageAccessHelper.isGranted()
        manageStorageStateTextView.text = getString(
            if (granted) {
                R.string.local_model_import_probe_manage_storage_granted
            } else {
                R.string.local_model_import_probe_manage_storage_missing
            }
        )
        pickFolderButton.isEnabled = granted
        pickFolderButton.alpha = if (granted) 1f else 0.5f
        grantManageStorageButton.visibility = if (granted) View.GONE else View.VISIBLE
    }

    private fun requestManageStorageAccess() {
        appendLog(getString(R.string.local_model_import_probe_manage_storage_request_started))
        val appIntent = ManageExternalStorageAccessHelper.buildAppSettingsIntent(packageName)
        val fallbackIntent = ManageExternalStorageAccessHelper.buildFallbackSettingsIntent()
        val intentToLaunch = when {
            appIntent.resolveActivity(packageManager) != null -> appIntent
            fallbackIntent.resolveActivity(packageManager) != null -> fallbackIntent
            else -> null
        }
        if (intentToLaunch == null) {
            appendLog(getString(R.string.local_model_import_probe_manage_storage_request_failed))
            Toast.makeText(
                this,
                R.string.local_model_import_probe_manage_storage_request_failed,
                Toast.LENGTH_SHORT
            ).show()
            return
        }
        manageStorageLauncher.launch(intentToLaunch)
    }

    private fun appendLog(message: String) {
        val existing = logTextView.text?.toString().orEmpty()
        val updated = if (existing.isEmpty()) message else "$existing\n$message"
        logTextView.text = updated
        scrollView.post { scrollView.fullScroll(android.view.View.FOCUS_DOWN) }
    }

    private fun runRuntimeProbe(modelDir: String) {
        statusTextView.text = "RUNNING_RUNTIME_PROBE"
        appendLog("runtimeProbe=starting")
        appendLog("runtimeProbe.modelDir=$modelDir")
        lifecycleScope.launch {
            val result = withContext(Dispatchers.IO) {
                performRuntimeProbe(modelDir)
            }
            statusTextView.text = result.status
            appendLog(result.log)
        }
    }

    private suspend fun performRuntimeProbe(modelDir: String): RuntimeProbeResult {
        val llmService = LlmService()
        return try {
            val initResult = llmService.init(modelDir)
            if (!initResult) {
                return RuntimeProbeResult(
                    status = "LOAD_OR_INFERENCE_FAILED",
                    log = "runtimeProbe.load=returned false"
                )
            }
            val firstChunk = withTimeout(45_000L) {
                llmService.generate("Say hi in one short sentence.")
                    .first { (_, aggregated) -> aggregated.isNotBlank() }
                    .second
            }
            llmService.requestStop()
            RuntimeProbeResult(
                status = "LOAD_AND_INFERENCE_PASSED",
                log = buildString {
                    appendLine("runtimeProbe.load=success")
                    appendLine("runtimeProbe.inference=success")
                    append("runtimeProbe.firstChunk=$firstChunk")
                }
            )
        } catch (error: Throwable) {
            RuntimeProbeResult(
                status = "LOAD_OR_INFERENCE_FAILED",
                log = buildString {
                    appendLine("runtimeProbe.error=${error::class.java.simpleName}")
                    append("runtimeProbe.message=${error.message}")
                }
            )
        } finally {
            runCatching { llmService.unload() }
        }
    }

    private data class RuntimeProbeResult(
        val status: String,
        val log: String
    )
}
