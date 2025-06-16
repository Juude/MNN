#!/bin/bash

# 用法: ./check_jni_symbol.sh Java_com_taobao_meta_avatar_tts_TtsService_nativeCreateTTS
# 默认检查 build/intermediates/cxx/Debug/*/obj/arm64-v8a/libmnn_tts.so

JNI_SYMBOL="$1"
SO_PATH=$(find ./build/intermediates/cxx/Debug -type f -name 'libmnn_tts.so' | head -n 1)

if [ -z "$JNI_SYMBOL" ]; then
  echo "用法: $0 <JNI_SYMBOL>"
  echo "例如: $0 Java_com_taobao_meta_avatar_tts_TtsService_nativeCreateTTS"
  exit 1
fi

# 编译 debug 包
./gradlew assembleDebug || { echo "Gradle build failed"; exit 2; }

if [ ! -f "$SO_PATH" ]; then
  echo "找不到 so 文件: $SO_PATH"
  exit 3
fi

echo "检查 $SO_PATH 中是否包含符号: $JNI_SYMBOL"
objdump -T "$SO_PATH" | grep "$JNI_SYMBOL" && echo "[OK] 找到符号 $JNI_SYMBOL" || echo "[ERROR] 未找到符号 $JNI_SYMBOL" 