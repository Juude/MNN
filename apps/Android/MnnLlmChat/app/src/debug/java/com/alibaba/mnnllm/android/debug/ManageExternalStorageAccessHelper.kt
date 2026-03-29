package com.alibaba.mnnllm.android.debug

import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.Settings

object ManageExternalStorageAccessHelper {

    fun isGranted(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Environment.isExternalStorageManager()
        } else {
            true
        }
    }

    fun requiresGrant(sdkInt: Int, isExternalStorageManager: Boolean): Boolean {
        return sdkInt >= Build.VERSION_CODES.R && !isExternalStorageManager
    }

    fun buildAppSettingsIntent(packageName: String): Intent {
        return Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION).apply {
            data = Uri.parse(buildAppSettingsUriString(packageName))
        }
    }

    fun buildFallbackSettingsIntent(): Intent {
        return Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)
    }

    fun buildAppSettingsUriString(packageName: String): String {
        return "package:$packageName"
    }
}
