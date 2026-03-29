package com.alibaba.mnnllm.android.debug

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ManageExternalStorageAccessHelperTest {

    @Test
    fun requiresGrant_isFalseBeforeAndroid11() {
        assertFalse(ManageExternalStorageAccessHelper.requiresGrant(29, false))
    }

    @Test
    fun requiresGrant_isTrueOnAndroid11WhenManagerMissing() {
        assertTrue(ManageExternalStorageAccessHelper.requiresGrant(30, false))
    }

    @Test
    fun requiresGrant_isFalseWhenManagerAlreadyGranted() {
        assertFalse(ManageExternalStorageAccessHelper.requiresGrant(34, true))
    }

    @Test
    fun buildAppSettingsUriString_targetsAppPackage() {
        val uriString = ManageExternalStorageAccessHelper.buildAppSettingsUriString("com.alibaba.mnnllm.android")

        assertEquals("package:com.alibaba.mnnllm.android", uriString)
    }
}
