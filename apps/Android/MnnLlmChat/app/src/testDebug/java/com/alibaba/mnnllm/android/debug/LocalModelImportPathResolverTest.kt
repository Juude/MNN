package com.alibaba.mnnllm.android.debug

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class LocalModelImportPathResolverTest {

    @Test
    fun parseExternalStorageDocumentId_resolvesPrimaryVolume() {
        val path = LocalModelImportPathResolver.parseExternalStorageDocumentId("primary:models/Qwen3")

        assertEquals("/storage/emulated/0/models/Qwen3", path)
    }

    @Test
    fun parseExternalStorageDocumentId_resolvesRemovableVolume() {
        val path = LocalModelImportPathResolver.parseExternalStorageDocumentId("0123-4567:models/Qwen3")

        assertEquals("/storage/0123-4567/models/Qwen3", path)
    }

    @Test
    fun parseExternalStorageDocumentId_rejectsMalformedId() {
        val path = LocalModelImportPathResolver.parseExternalStorageDocumentId("primary")

        assertNull(path)
    }
}
