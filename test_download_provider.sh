#!/bin/bash

echo "🧪 Testing MNN CLI Download Provider Configuration"
echo "=================================================="

echo ""
echo "1. Show current configuration:"
mnncli config show

echo ""
echo "2. Show system information:"
mnncli info

echo ""
echo "3. Set download provider to ModelScope:"
mnncli config set download_provider modelscope

echo ""
echo "4. Show updated configuration:"
mnncli config show

echo ""
echo "5. Set download provider to Modelers:"
mnncli config set download_provider modelers

echo ""
echo "6. Show updated configuration:"
mnncli config show

echo ""
echo "7. Reset to HuggingFace:"
mnncli config set download_provider huggingface

echo ""
echo "8. Show final configuration:"
mnncli config show

echo ""
echo "9. Show configuration help:"
mnncli config help

echo ""
echo "✅ Test completed!"
echo ""
echo "To test with environment variables:"
echo "export MNN_DOWNLOAD_PROVIDER=modelscope"
echo "mnncli config show"
echo "mnncli info"
