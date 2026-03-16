#!/usr/bin/env bash
set -euo pipefail

# Vision Chat UI Smoke Test for PR #4263
#
# Tests the real-time vision capabilities added in PR #4263:
# - Navigate to chat → open voice chat → verify camera UI elements
# - Camera preview (PreviewView), camera toggle, camera switch (front/back)
# - Echo cancellation mode button
#
# Prerequisites:
# - APK with CameraX support installed
# - At least one LLM model downloaded (e.g. Qwen3.5-0.8B-MNN)
# - Camera + Audio permissions granted
#
# Usage:
#   ./scripts/19_regress_vision_chat_ui.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SMOKE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ARTIFACT_DIR="${ARTIFACT_DIR:-$SMOKE_DIR/artifacts}"
OUT_DIR="$ARTIFACT_DIR/vision_chat"
SHOT_DIR="$OUT_DIR/shots"
mkdir -p "$OUT_DIR" "$SHOT_DIR"

QUERY="$SCRIPT_DIR/ui_xml_query.py"
PACKAGE_NAME="${PACKAGE_NAME:-com.alibaba.mnnllm.android}"
TMP_XML="/tmp/mnn_vision_ui.xml"
UI_LOG="$OUT_DIR/ui_actions.log"
SUMMARY_FILE="$OUT_DIR/summary.txt"

VISION_WAIT_SEC="${VISION_WAIT_SEC:-12}"

: >"$UI_LOG"

log() { echo "$(date '+%H:%M:%S') $*" | tee -a "$UI_LOG"; }

ensure_device_unlocked() {
  adb shell input keyevent 224 >/dev/null 2>&1 || true
  adb shell wm dismiss-keyguard >/dev/null 2>&1 || true
  adb shell input keyevent 82 >/dev/null 2>&1 || true
  adb shell input swipe 540 2000 540 700 200 >/dev/null 2>&1 || true
  sleep 1
}

shot() {
  local name="$1"
  adb exec-out screencap -p >"$SHOT_DIR/${name}.png"
  log "SHOT ${name}.png"
}

dump_ui() {
  local out="$1"
  local try
  for try in 1 2 3; do
    adb shell uiautomator dump /sdcard/mnn_vision_ui.xml >/dev/null 2>/dev/null || true
    adb pull /sdcard/mnn_vision_ui.xml "$out" >/dev/null 2>/dev/null || true
    if [ -s "$out" ] && rg -q "<hierarchy" "$out"; then
      # Handle MIUI permission overlay
      if rg -q "package=\"com\\.lbe\\.security\\.miui\"" "$out"; then
        log "MIUI_PERM try=$try"
        tap_by_any_contains_text "$out" "始终允许" "仅在使用中允许" "允许" "同意" || true
        sleep 1; continue
      fi
      # Handle permission dialogs
      if rg -q "允许.*录音\|allow.*record\|RECORD_AUDIO\|允许.*相机\|allow.*camera\|CAMERA" "$out"; then
        log "PERM_DIALOG try=$try"
        tap_by_any_contains_text "$out" "允许" "Allow" "始终允许" "While using the app" || true
        sleep 1; continue
      fi
      if rg -q "package=\"com\\.alibaba\\.mnnllm\\.android" "$out"; then
        return 0
      fi
    fi
    log "DUMP_RETRY try=$try"
    ensure_device_unlocked
    sleep 1
  done
  return 1
}

tap_by_any_rid() {
  local xml="$1"; shift
  local rid
  for rid in "$@"; do
    if python3 "$QUERY" --xml "$xml" --resource-id "$rid" >/tmp/mnn_query_hit.txt 2>/dev/null; then
      read -r x y _ </tmp/mnn_query_hit.txt
      log "TAP rid=$(echo "$rid" | sed 's/.*:id\///' ) x=$x y=$y"
      adb shell input tap "$x" "$y"
      return 0
    fi
  done
  return 1
}

tap_by_any_contains_text() {
  local xml="$1"; shift
  local text
  for text in "$@"; do
    if python3 "$QUERY" --xml "$xml" --contains-text "$text" >/tmp/mnn_query_hit.txt 2>/dev/null; then
      read -r x y _ </tmp/mnn_query_hit.txt
      log "TAP text='$text' x=$x y=$y"
      adb shell input tap "$x" "$y"
      return 0
    fi
  done
  return 1
}

exists_any_rid() {
  local xml="$1"; shift
  local rid
  for rid in "$@"; do
    python3 "$QUERY" --xml "$xml" --resource-id "$rid" >/dev/null 2>&1 && return 0
  done
  return 1
}

exists_any_text() {
  local xml="$1"; shift
  local text
  for text in "$@"; do
    python3 "$QUERY" --xml "$xml" --contains-text "$text" >/dev/null 2>&1 && return 0
  done
  return 1
}

open_overflow_menu() {
  adb shell input keyevent 82 >/dev/null 2>&1 || true
  sleep 1
  dump_ui "$TMP_XML" || true
}

wait_for_chat_ready() {
  local attempt
  for attempt in 1 2 3 4 5 6; do
    sleep 2
    dump_ui "$TMP_XML" || true
    if exists_any_rid "$TMP_XML" \
      "com.alibaba.mnnllm.android:id/et_message" \
      "com.alibaba.mnnllm.android.release:id/et_message" \
      "com.alibaba.mnnllm.android:id/more_item_voice_chat" \
      "com.alibaba.mnnllm.android.release:id/more_item_voice_chat"; then
      log "CHAT_READY attempt=$attempt"
      return 0
    fi
    log "CHAT_READY_RETRY attempt=$attempt"
  done
  return 1
}

# ---------- Result tracking ----------
enter_chat_ok="FAIL"
open_voice_ok="FAIL"
voice_init_ok="FAIL"
camera_preview_ok="FAIL"
camera_button_ok="FAIL"
camera_switch_ok="FAIL"
echo_cancel_ok="FAIL"
camera_toggle_ok="SKIP"
end_voice_ok="FAIL"

log "=== Vision Chat UI Smoke (PR #4263) ==="
ensure_device_unlocked

# ===== Step 1: Launch app and enter chat =====
log "Step 1: Launch app + enter chat"
adb shell am force-stop "$PACKAGE_NAME"
sleep 1
adb shell monkey -p "$PACKAGE_NAME" -c android.intent.category.LAUNCHER 1 >/dev/null
sleep 3
dump_ui "$TMP_XML"
shot "01_launch"

# Navigate to first downloaded model chat
# Try tapping model name or "对话" button
if tap_by_any_contains_text "$TMP_XML" "对话" "Chat"; then
  sleep 2
  wait_for_chat_ready || true
  dump_ui "$TMP_XML"
elif tap_by_any_contains_text "$TMP_XML" "Qwen" "qwen"; then
  sleep 2
  dump_ui "$TMP_XML"
  # May need to tap "对话" on model detail page
  tap_by_any_contains_text "$TMP_XML" "对话" "Chat" || true
  sleep 2
  wait_for_chat_ready || true
  dump_ui "$TMP_XML"
fi

shot "02_chat"

if exists_any_rid "$TMP_XML" \
  "com.alibaba.mnnllm.android:id/et_message" \
  "com.alibaba.mnnllm.android.release:id/et_message" \
  "com.alibaba.mnnllm.android:id/more_item_voice_chat" \
  "com.alibaba.mnnllm.android.release:id/more_item_voice_chat"; then
  enter_chat_ok="PASS"
  log "ENTER_CHAT: PASS"
else
  log "ENTER_CHAT: FAIL – trying toolbar phone icon"
  # Some layouts have voice chat directly in toolbar
  if exists_any_text "$TMP_XML" "帮忙" "help"; then
    enter_chat_ok="PASS"
    log "ENTER_CHAT: PASS (alt)"
  fi
fi

cp "$TMP_XML" "$OUT_DIR/chat_entered.xml"

# ===== Step 2: Open Voice Chat =====
log "Step 2: Open voice chat"
if [ "$enter_chat_ok" = "PASS" ]; then
  # Try direct voice chat buttons
  if tap_by_any_rid "$TMP_XML" \
    "com.alibaba.mnnllm.android:id/more_item_voice_chat" \
    "com.alibaba.mnnllm.android.release:id/more_item_voice_chat" \
    "com.alibaba.mnnllm.android:id/start_voice_chat" \
    "com.alibaba.mnnllm.android.release:id/start_voice_chat"; then
    sleep 2
    dump_ui "$TMP_XML"
    open_voice_ok="PASS"
  # Try overflow menu
  elif open_overflow_menu && tap_by_any_contains_text "$TMP_XML" \
    "Start Voice Chat" "开启语音会话" "Voice Chat" "语音会话" "语音聊天"; then
    sleep 2
    dump_ui "$TMP_XML"
    open_voice_ok="PASS"
  # Toolbar fallback (phone icon at top right)
  else
    log "Trying toolbar action items..."
    dump_ui "$TMP_XML"
    # Look for phone/voice icon in toolbar by content-desc
    if tap_by_any_contains_text "$TMP_XML" "Voice" "语音" "通话"; then
      sleep 2
      dump_ui "$TMP_XML"
      open_voice_ok="PASS"
    fi
  fi
fi

shot "03_voice_chat_opening"
cp "$TMP_XML" "$OUT_DIR/voice_opening.xml"

# ===== Step 3: Wait for voice chat to initialize, handle permissions =====
log "Step 3: Wait for voice chat init"
if [ "$open_voice_ok" = "PASS" ]; then
  # Handle permission dialogs
  for _ in 1 2 3; do
    dump_ui "$TMP_XML"
    if rg -q "允许\|Allow\|permission" "$TMP_XML" 2>/dev/null; then
      tap_by_any_contains_text "$TMP_XML" "允许" "Allow" "始终允许" "While using" || true
      sleep 1
    else
      break
    fi
  done

  wait_count=0
  while [ "$wait_count" -lt "$VISION_WAIT_SEC" ]; do
    dump_ui "$TMP_XML"

    if exists_any_rid "$TMP_XML" \
      "com.alibaba.mnnllm.android:id/button_end_call" \
      "com.alibaba.mnnllm.android.release:id/button_end_call" \
      "com.alibaba.mnnllm.android:id/text_status" \
      "com.alibaba.mnnllm.android.release:id/text_status" \
      "com.alibaba.mnnllm.android:id/button_echo_cancel_mode" \
      "com.alibaba.mnnllm.android.release:id/button_echo_cancel_mode"; then
      voice_init_ok="PASS"
      log "VOICE_INIT: PASS (wait=$wait_count)"
      break
    fi

    log "VOICE_INIT_WAIT $wait_count/$VISION_WAIT_SEC"
    shot "04_voice_wait_${wait_count}"
    sleep 1
    wait_count=$((wait_count + 1))
  done
fi

shot "05_voice_chat_active"
cp "$TMP_XML" "$OUT_DIR/voice_active.xml"

# ===== Step 4: Verify PR #4263 camera UI elements =====
log "Step 4: Verify vision UI elements"
dump_ui "$TMP_XML"

# 4a. Camera Button (visible before toggle, camera starts OFF)
if exists_any_rid "$TMP_XML" \
  "com.alibaba.mnnllm.android:id/button_camera" \
  "com.alibaba.mnnllm.android.release:id/button_camera"; then
  camera_button_ok="PASS"
  log "CAMERA_BUTTON: PASS"
else
  log "CAMERA_BUTTON: FAIL"
fi

# 4b. Echo Cancellation Mode
if exists_any_rid "$TMP_XML" \
  "com.alibaba.mnnllm.android:id/button_echo_cancel_mode" \
  "com.alibaba.mnnllm.android.release:id/button_echo_cancel_mode"; then
  echo_cancel_ok="PASS"
  log "ECHO_CANCEL: PASS"
else
  log "ECHO_CANCEL: FAIL"
fi

shot "06_vision_elements"

# ===== Step 5: Toggle camera ON and verify preview + switch =====
log "Step 5: Toggle camera ON"
if [ "$camera_button_ok" = "PASS" ]; then
  if tap_by_any_rid "$TMP_XML" \
    "com.alibaba.mnnllm.android:id/button_camera" \
    "com.alibaba.mnnllm.android.release:id/button_camera"; then
    sleep 3  # wait for CameraX to start
    dump_ui "$TMP_XML"
    shot "07_camera_toggled"

    # 5a. Camera Preview should now be visible
    if exists_any_rid "$TMP_XML" \
      "com.alibaba.mnnllm.android:id/camera_preview" \
      "com.alibaba.mnnllm.android.release:id/camera_preview"; then
      camera_preview_ok="PASS"
      camera_toggle_ok="PASS"
      log "CAMERA_PREVIEW: PASS (visible after toggle)"
      log "CAMERA_TOGGLE: PASS"
    else
      camera_toggle_ok="FAIL"
      log "CAMERA_PREVIEW: FAIL (not visible after toggle)"
      log "CAMERA_TOGGLE: FAIL"
    fi

    # 5b. Camera Switch should appear when camera is active
    if exists_any_rid "$TMP_XML" \
      "com.alibaba.mnnllm.android:id/button_camera_switch" \
      "com.alibaba.mnnllm.android.release:id/button_camera_switch"; then
      camera_switch_ok="PASS"
      log "CAMERA_SWITCH: PASS (visible when camera active)"
    else
      log "CAMERA_SWITCH: FAIL (not visible when camera active)"
    fi
  fi
else
  log "CAMERA_TOGGLE: SKIP (no button)"
fi

# ===== Step 6: End voice chat =====
log "Step 6: End voice chat"
dump_ui "$TMP_XML"
if tap_by_any_rid "$TMP_XML" \
  "com.alibaba.mnnllm.android:id/button_end_call" \
  "com.alibaba.mnnllm.android.release:id/button_end_call"; then
  sleep 2
  dump_ui "$TMP_XML"
  if exists_any_rid "$TMP_XML" \
    "com.alibaba.mnnllm.android:id/et_message" \
    "com.alibaba.mnnllm.android.release:id/et_message"; then
    end_voice_ok="PASS"
    log "END_VOICE: PASS (back to chat)"
  else
    end_voice_ok="PASS"
    log "END_VOICE: PASS (call ended)"
  fi
else
  adb shell input keyevent 4
  sleep 2
  end_voice_ok="PARTIAL"
  log "END_VOICE: PARTIAL (back pressed)"
fi
shot "08_after_end"

# ===== Summary =====
overall="FAIL"
if [ "$enter_chat_ok" = "PASS" ] && [ "$open_voice_ok" = "PASS" ] && [ "$voice_init_ok" = "PASS" ]; then
  overall="PASS"
fi

{
  echo "======================================"
  echo "  PR #4263 Vision Chat Smoke Results"
  echo "======================================"
  echo ""
  echo "OVERALL=$overall"
  echo ""
  echo "--- Navigation ---"
  echo "ENTER_CHAT=$enter_chat_ok"
  echo "OPEN_VOICE_CHAT=$open_voice_ok"
  echo "VOICE_CHAT_INIT=$voice_init_ok"
  echo "END_VOICE_CHAT=$end_voice_ok"
  echo ""
  echo "--- PR #4263 Vision Features ---"
  echo "CAMERA_PREVIEW=$camera_preview_ok"
  echo "CAMERA_BUTTON=$camera_button_ok"
  echo "CAMERA_SWITCH=$camera_switch_ok"
  echo "ECHO_CANCEL_MODE=$echo_cancel_ok"
  echo "CAMERA_TOGGLE=$camera_toggle_ok"
  echo ""
  echo "SHOTS=$SHOT_DIR"
  echo "LOG=$UI_LOG"
} | tee "$SUMMARY_FILE"

log "=== Done ==="

if [ "$overall" != "PASS" ]; then
  exit 1
fi
