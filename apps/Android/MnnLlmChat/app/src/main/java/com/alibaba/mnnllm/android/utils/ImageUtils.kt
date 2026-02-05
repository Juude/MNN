package com.alibaba.mnnllm.android.utils

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.view.LayoutInflater
import android.view.View
import android.widget.Toast
import com.alibaba.mnnllm.android.R
import com.google.android.material.bottomsheet.BottomSheetDialog
import java.io.File

object ImageUtils {
    
    fun showImageMenu(context: Context, imageUri: Uri) {
        val dialog = BottomSheetDialog(context)
        val view = LayoutInflater.from(context).inflate(R.layout.bottom_sheet_image_menu, null)
        
        view.findViewById<View>(R.id.btn_save_image).setOnClickListener {
            dialog.dismiss()
            val filePath = FileUtils.getPathForUri(imageUri) ?: imageUri.path ?: return@setOnClickListener
            val file = File(filePath)
            if (file.exists()) {
                val success = FileUtils.saveImageToGallery(context, file)
                if (success) {
                    Toast.makeText(context, R.string.image_saved_to_gallery, Toast.LENGTH_SHORT).show()
                } else {
                    Toast.makeText(context, R.string.failed_to_save_image, Toast.LENGTH_SHORT).show()
                }
            } else {
                Toast.makeText(context, R.string.failed_to_save_image, Toast.LENGTH_SHORT).show()
            }
        }

        view.findViewById<View>(R.id.btn_share_image).setOnClickListener {
            dialog.dismiss()
            shareImage(context, imageUri)
        }

        dialog.show()
        dialog.setContentView(view)
    }

    fun shareImage(context: Context, imageUri: Uri) {
        val shareUri = if (imageUri.scheme == "file") {
            androidx.core.content.FileProvider.getUriForFile(
                context,
                context.packageName + ".fileprovider",
                File(imageUri.path!!)
            )
        } else {
            imageUri
        }
        
        val shareIntent = Intent(Intent.ACTION_SEND).apply {
            type = "image/*"
            putExtra(Intent.EXTRA_STREAM, shareUri)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        context.startActivity(Intent.createChooser(shareIntent, context.getString(R.string.share_image)))
    }
}
