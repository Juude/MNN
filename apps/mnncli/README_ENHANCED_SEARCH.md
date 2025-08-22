# Enhanced Model Search for MNN CLI

This document describes the enhanced model search functionality that integrates both ModelRepository data and HuggingFace API capabilities.

## Overview

The enhanced search system provides a unified interface for searching models across multiple platforms:
- **Primary**: ModelRepository data (from model_market.json)
- **Fallback**: HuggingFace API search
- **Automatic platform detection** based on model ID prefixes

## Features

### 1. Multi-Platform Support
- **HuggingFace**: `HuggingFace/` or `hf/` prefix
- **ModelScope**: `ModelScope/` or `ms/` prefix  
- **Modelers**: `Modelers/` or `ml/` prefix
- **Default**: HuggingFace if no prefix

### 2. Enhanced Search Results
- Model name and vendor information
- Model size (parameters and file size)
- Tags and categories
- Source platform information
- Download instructions

### 3. Environment Configuration
Set preferred download source via environment variable:
```bash
export MNN_DOWNLOAD_SOURCE="ModelScope"  # or "HuggingFace", "Modelers"
```

## Usage

### Basic Search
```bash
# Search for models with keyword
mnncli model search qwen

# Search with specific platform preference
MNN_DOWNLOAD_SOURCE=ModelScope mnncli model search qwen
```

### Search Results Format
```
Found 3 models:
  🔍 Qwen-1.8B-Chat (MNN) [1.8B] - ModelScope - 0.5GB
     Tags: chat, qwen, 1.8b, int4
  🔍 Qwen-7B-Chat (MNN) [7B] - ModelScope - 1.2GB
     Tags: chat, qwen, 7b, int4
  🔍 Qwen-14B-Chat (MNN) [14B] - ModelScope - 2.1GB
     Tags: chat, qwen, 14b, int4

To download a model, use: mnncli model download <model_name>
```

### Download Models
```bash
# Download from specific platform
mnncli model download ModelScope/MNN/Qwen-1.8B-Chat-Int4

# Download with automatic platform detection
mnncli model download qwen-1.8b-chat-int4
```

## Architecture

### Components

1. **EnhancedModelSearcher**: Main search interface
   - Integrates ModelRepository and HfApiClient
   - Provides unified search results
   - Handles platform detection and fallback

2. **ModelMarketLoader**: Data loading from various sources
   - Local cache files
   - Embedded assets
   - Network URLs

3. **RemoteModelDownloader**: Multi-platform download support
   - Platform-specific clients
   - Unified download interface
   - Automatic retry and error handling

### Data Flow

```
User Search Request
       ↓
EnhancedModelSearcher
       ↓
1. Try ModelRepository (cache/assets/network)
       ↓
2. If no results, fallback to HuggingFace API
       ↓
3. Convert results to unified format
       ↓
4. Display enhanced search results
```

## Configuration

### Environment Variables
- `MNN_DOWNLOAD_SOURCE`: Preferred download platform
- `HF_ENDPOINT`: Custom HuggingFace endpoint
- `MNN_CACHE_DIR`: Custom cache directory

### Cache Management
- Model market data is cached locally
- Cache validity: 1 hour
- Automatic fallback to assets if cache fails

## Future Enhancements

1. **JSON Parsing**: Implement full JSON parsing for model market data
2. **Network Loading**: Add network fetching for model market data
3. **Asset Embedding**: Embed model market data in binary
4. **Advanced Filtering**: Add more search filters (size, tags, vendor)
5. **Real-time Updates**: Periodic refresh of model market data

## Troubleshooting

### Common Issues

1. **"No models found"**: 
   - Check network connectivity
   - Verify cache file exists and is valid
   - Try different search keywords

2. **"Failed to search models"**:
   - Check error messages for specific issues
   - Verify environment variables are set correctly
   - Check cache directory permissions

3. **Search returns HuggingFace results only**:
   - Model market data may not be available
   - Check if cache file exists
   - Verify JSON parsing is working

### Debug Mode
Enable debug output by setting environment variable:
```bash
export MNN_DEBUG=1
mnncli model search qwen
```

## Integration with Existing Code

The enhanced search system is designed to work alongside existing functionality:
- **HfApiClient**: Preserved for backward compatibility
- **RemoteModelDownloader**: Enhanced with multi-platform support
- **FileUtils**: Integrated for cache management
- **Existing commands**: All existing CLI commands continue to work

This ensures a smooth transition while providing enhanced capabilities for users who need them.
