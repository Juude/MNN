package com.alibaba.mnn.tts

class MNNTTS {
    companion object {
        init {
            try {
                System.loadLibrary("mnn_tts")
            } catch (e: UnsatisfiedLinkError) {
                e.printStackTrace()
            }
        }

        @JvmStatic
        external fun getHelloWorldFromJNI(): String
    }
}