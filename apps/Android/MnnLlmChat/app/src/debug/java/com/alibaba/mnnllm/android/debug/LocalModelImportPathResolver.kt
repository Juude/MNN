package com.alibaba.mnnllm.android.debug

import android.content.Context
import android.net.Uri
import android.provider.DocumentsContract
import android.provider.DocumentsContract.Document
import com.alibaba.mnnllm.android.modelsettings.ModelConfig
import java.io.File

object LocalModelImportPathResolver {

    private const val EXTERNAL_STORAGE_AUTHORITY = "com.android.externalstorage.documents"

    enum class ProbeStatus {
        RESOLVED_AND_PRECHECK_PASSED,
        RESOLVED_BUT_PATH_ACCESS_DENIED,
        UNSUPPORTED_PROVIDER,
        RESOLVED_BUT_INVALID_MODEL_DIR
    }

    data class ArtifactCheck(
        val fieldName: String,
        val configuredValue: String,
        val resolvedPath: String,
        val exists: Boolean
    )

    data class ProbeReport(
        val treeUri: String,
        val authority: String?,
        val documentId: String?,
        val resolvedPath: String?,
        val status: ProbeStatus,
        val summary: String,
        val configPath: String? = null,
        val treeConfigVisible: Boolean? = null,
        val treeConfigUri: String? = null,
        val treeQueryError: String? = null,
        val directConfigReadable: Boolean? = null,
        val directConfigReadError: String? = null,
        val artifactChecks: List<ArtifactCheck> = emptyList()
    ) {
        fun toMultilineString(): String = buildString {
            appendLine("status=$status")
            appendLine("summary=$summary")
            appendLine("treeUri=$treeUri")
            appendLine("authority=${authority ?: "(null)"}")
            appendLine("documentId=${documentId ?: "(null)"}")
            appendLine("resolvedPath=${resolvedPath ?: "(null)"}")
            appendLine("configPath=${configPath ?: "(null)"}")
            appendLine("treeConfigVisible=${treeConfigVisible ?: "(null)"}")
            appendLine("treeConfigUri=${treeConfigUri ?: "(null)"}")
            appendLine("treeQueryError=${treeQueryError ?: "(null)"}")
            appendLine("directConfigReadable=${directConfigReadable ?: "(null)"}")
            appendLine("directConfigReadError=${directConfigReadError ?: "(null)"}")
            if (artifactChecks.isNotEmpty()) {
                appendLine("artifacts:")
                artifactChecks.forEach { check ->
                    appendLine(
                        "  - ${check.fieldName}: ${check.configuredValue} -> ${check.resolvedPath} (exists=${check.exists})"
                    )
                }
            }
        }.trimEnd()
    }

    fun inspect(context: Context, treeUri: Uri): ProbeReport {
        val authority = treeUri.authority
        val documentId = runCatching { DocumentsContract.getTreeDocumentId(treeUri) }.getOrNull()
        val treeConfigCheck = inspectTreeConfig(context, treeUri, documentId)
        val resolvedPath = when (authority) {
            EXTERNAL_STORAGE_AUTHORITY -> documentId?.let(::parseExternalStorageDocumentId)
            else -> null
        }

        if (resolvedPath == null) {
            val summary = when {
                authority == EXTERNAL_STORAGE_AUTHORITY -> "ExternalStorage documentId is not convertible to a direct path"
                else -> "Unsupported provider authority: ${authority ?: "(null)"}"
            }
            return ProbeReport(
                treeUri = treeUri.toString(),
                authority = authority,
                documentId = documentId,
                resolvedPath = null,
                status = ProbeStatus.UNSUPPORTED_PROVIDER,
                summary = summary,
                treeConfigVisible = treeConfigCheck.configVisible,
                treeConfigUri = treeConfigCheck.configUri,
                treeQueryError = treeConfigCheck.error
            )
        }

        return validateModelDirectory(
            treeUri = treeUri,
            authority = authority,
            documentId = documentId,
            resolvedPath = resolvedPath,
            treeConfigCheck = treeConfigCheck
        )
    }

    fun parseExternalStorageDocumentId(documentId: String): String? {
        val separator = documentId.indexOf(':')
        if (separator <= 0) {
            return null
        }
        val volume = documentId.substring(0, separator)
        val relativePath = documentId.substring(separator + 1).trimStart('/')
        val basePath = when {
            volume.equals("primary", ignoreCase = true) -> "/storage/emulated/0"
            volume.matches(Regex("[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}")) -> "/storage/$volume"
            else -> return null
        }
        return if (relativePath.isEmpty()) basePath else "$basePath/$relativePath"
    }

    private fun validateModelDirectory(
        treeUri: Uri,
        authority: String?,
        documentId: String?,
        resolvedPath: String,
        treeConfigCheck: TreeConfigCheck
    ): ProbeReport {
        val modelDir = File(resolvedPath)
        if (!modelDir.isDirectory) {
            return ProbeReport(
                treeUri = treeUri.toString(),
                authority = authority,
                documentId = documentId,
                resolvedPath = resolvedPath,
                status = ProbeStatus.RESOLVED_BUT_INVALID_MODEL_DIR,
                summary = "Resolved path is not a readable directory",
                treeConfigVisible = treeConfigCheck.configVisible,
                treeConfigUri = treeConfigCheck.configUri,
                treeQueryError = treeConfigCheck.error
            )
        }

        val modelId = "local/$resolvedPath"
        val configPath = ModelConfig.getDefaultConfigFile(modelId)
        if (configPath.isNullOrBlank()) {
            return ProbeReport(
                treeUri = treeUri.toString(),
                authority = authority,
                documentId = documentId,
                resolvedPath = resolvedPath,
                status = ProbeStatus.RESOLVED_BUT_INVALID_MODEL_DIR,
                summary = if (treeConfigCheck.configVisible == true) {
                    "config.json is visible via picker Uri, but path-based lookup could not access it directly"
                } else {
                    "config.json is missing or not directly readable"
                },
                configPath = null,
                treeConfigVisible = treeConfigCheck.configVisible,
                treeConfigUri = treeConfigCheck.configUri,
                treeQueryError = treeConfigCheck.error
            )
        }

        val directConfigAccess = inspectDirectConfigAccess(File(configPath))
        if (!directConfigAccess.readable) {
            return ProbeReport(
                treeUri = treeUri.toString(),
                authority = authority,
                documentId = documentId,
                resolvedPath = resolvedPath,
                status = ProbeStatus.RESOLVED_BUT_PATH_ACCESS_DENIED,
                summary = "config.json exists, but direct file-path access is denied; picker Uri permission did not become native-readable path access",
                configPath = configPath,
                treeConfigVisible = treeConfigCheck.configVisible,
                treeConfigUri = treeConfigCheck.configUri,
                treeQueryError = treeConfigCheck.error,
                directConfigReadable = false,
                directConfigReadError = directConfigAccess.error
            )
        }

        val config = ModelConfig.loadConfig(modelId)
        if (config == null) {
            return ProbeReport(
                treeUri = treeUri.toString(),
                authority = authority,
                documentId = documentId,
                resolvedPath = resolvedPath,
                status = ProbeStatus.RESOLVED_BUT_INVALID_MODEL_DIR,
                summary = "config.json exists but ModelConfig.loadConfig failed",
                configPath = configPath,
                treeConfigVisible = treeConfigCheck.configVisible,
                treeConfigUri = treeConfigCheck.configUri,
                treeQueryError = treeConfigCheck.error,
                directConfigReadable = true
            )
        }

        val artifactChecks = listOfNotNull(
            buildArtifactCheck(modelDir, "llm_model", config.llmModel),
            buildArtifactCheck(modelDir, "llm_weight", config.llmWeight),
            buildArtifactCheck(modelDir, "visual_model", config.visualModel)
        )
        val missingArtifacts = artifactChecks.filterNot { it.exists }

        return ProbeReport(
            treeUri = treeUri.toString(),
            authority = authority,
            documentId = documentId,
            resolvedPath = resolvedPath,
            status = if (missingArtifacts.isEmpty()) {
                ProbeStatus.RESOLVED_AND_PRECHECK_PASSED
            } else {
                ProbeStatus.RESOLVED_BUT_INVALID_MODEL_DIR
            },
            summary = if (missingArtifacts.isEmpty()) {
                "Resolved path passed current local-model precheck"
            } else {
                "Resolved path is readable but config references missing files"
            },
            configPath = configPath,
            treeConfigVisible = treeConfigCheck.configVisible,
            treeConfigUri = treeConfigCheck.configUri,
            treeQueryError = treeConfigCheck.error,
            directConfigReadable = true,
            artifactChecks = artifactChecks
        )
    }

    private fun inspectTreeConfig(
        context: Context,
        treeUri: Uri,
        documentId: String?
    ): TreeConfigCheck {
        if (documentId.isNullOrBlank()) {
            return TreeConfigCheck(configVisible = false, error = "tree documentId is missing")
        }
        val childrenUri = runCatching {
            DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, documentId)
        }.getOrElse { error ->
            return TreeConfigCheck(configVisible = false, error = "${error::class.java.simpleName}: ${error.message}")
        }
        return try {
            context.contentResolver.query(
                childrenUri,
                arrayOf(Document.COLUMN_DISPLAY_NAME, Document.COLUMN_DOCUMENT_ID),
                null,
                null,
                null
            )?.use { cursor ->
                val nameIndex = cursor.getColumnIndex(Document.COLUMN_DISPLAY_NAME)
                val idIndex = cursor.getColumnIndex(Document.COLUMN_DOCUMENT_ID)
                while (cursor.moveToNext()) {
                    if (nameIndex == -1) {
                        continue
                    }
                    val displayName = cursor.getString(nameIndex)
                    if (displayName == "config.json") {
                        val childDocumentId = if (idIndex != -1) cursor.getString(idIndex) else null
                        val childUri = childDocumentId?.let {
                            DocumentsContract.buildDocumentUriUsingTree(treeUri, it).toString()
                        }
                        return TreeConfigCheck(configVisible = true, configUri = childUri)
                    }
                }
                TreeConfigCheck(configVisible = false)
            } ?: TreeConfigCheck(configVisible = false, error = "contentResolver.query returned null cursor")
        } catch (error: Throwable) {
            TreeConfigCheck(configVisible = false, error = "${error::class.java.simpleName}: ${error.message}")
        }
    }

    private fun inspectDirectConfigAccess(configFile: File): DirectConfigAccessCheck {
        return try {
            configFile.inputStream().use { inputStream ->
                inputStream.read()
            }
            DirectConfigAccessCheck(readable = true, error = null)
        } catch (error: Throwable) {
            DirectConfigAccessCheck(
                readable = false,
                error = "${error::class.java.simpleName}: ${error.message}"
            )
        }
    }

    private fun buildArtifactCheck(
        modelDir: File,
        fieldName: String,
        configuredValue: String?
    ): ArtifactCheck? {
        val value = configuredValue?.trim().orEmpty()
        if (value.isEmpty()) {
            return null
        }
        val resolvedFile = File(value).takeIf { it.isAbsolute } ?: File(modelDir, value)
        return ArtifactCheck(
            fieldName = fieldName,
            configuredValue = value,
            resolvedPath = resolvedFile.absolutePath,
            exists = resolvedFile.exists()
        )
    }

    private data class TreeConfigCheck(
        val configVisible: Boolean,
        val configUri: String? = null,
        val error: String? = null
    )

    private data class DirectConfigAccessCheck(
        val readable: Boolean,
        val error: String? = null
    )
}
