# MNNCLI Android Build Guide with SSL Support

This guide explains how to compile mnncli to an Android command line app and handle SSL for HTTPS clients on Android.

## Prerequisites

1. **Android NDK** - Download and install Android NDK (recommended version 25+)
2. **Set environment variable**:
   ```bash
   export ANDROID_NDK=/path/to/your/android-ndk
   ```

## Building MNNCLI for Android

### Method 1: Using the provided build script (Recommended)

```bash
cd apps/mnncli
./build_android.sh
```

### Method 2: Manual build

```bash
# Create build directory
mkdir -p build_mnncli_android
cd build_mnncli_android

# Configure CMake
cmake ../../.. \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DANDROID_ABI="arm64-v8a" \
    -DANDROID_STL=c++_static \
    -DANDROID_NATIVE_API_LEVEL=android-21 \
    -DMNN_BUILD_FOR_ANDROID_COMMAND=true \
    -DMNN_USE_LOGCAT=false \
    -DMNN_BUILD_LLM=ON \
    -DBUILD_MNNCLI=ON \
    -DNATIVE_LIBRARY_OUTPUT=. \
    -DNATIVE_INCLUDE_OUTPUT=.

# Build
make -j$(nproc)
```

## SSL/HTTPS Support on Android

### Current Implementation

The mnncli application uses `httplib` with OpenSSL support for HTTPS requests. The current code in `ml_api_client.cpp` shows:

```cpp
httplib::SSLClient cli(host_, 443);
```

### SSL Configuration for Android

#### 1. OpenSSL Integration

The CMakeLists.txt already includes OpenSSL support:
```cmake
if(OpenSSL_FOUND)
    target_include_directories(mnncli PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(mnncli PRIVATE ${OPENSSL_LIBRARIES})
    target_compile_definitions(mnncli PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
endif()
```

#### 2. Android-Specific SSL Considerations

**Certificate Store**: Android uses its own certificate store. For production apps, you may need to:
- Bundle your own CA certificates
- Handle certificate pinning
- Manage certificate validation

**SSL Context Configuration**: Consider adding SSL context configuration:

```cpp
// Example SSL context configuration for Android
httplib::SSLClient cli(host_, 443);
cli.set_ca_cert_path("/system/etc/security/cacerts"); // Android system CA store
cli.enable_server_certificate_verification(true);
```

#### 3. Network Security Configuration

For Android 7.0+ (API level 24+), consider network security configuration:

```xml
<!-- res/xml/network_security_config.xml -->
<network-security-config>
    <domain-config cleartextTrafficPermitted="false">
        <domain includeSubdomains="true">modelers.cn</domain>
    </domain-config>
</network-security-config>
```

## Deployment and Usage

### 1. Copy to Android Device

```bash
# Copy the built executable to your Android device
adb push build_mnncli_android/apps/mnncli/mnncli /data/local/tmp/

# Set permissions
adb shell chmod +x /data/local/tmp/mnncli
```

### 2. Run on Android Device

```bash
# Connect to device shell
adb shell

# Navigate to executable location
cd /data/local/tmp

# Run mnncli
./mnncli --help
```

### 3. Test HTTPS Functionality

```bash
# Test the ML API client functionality
./mnncli info
./mnncli config show
```

## Troubleshooting

### Common Issues

1. **OpenSSL not found**: Ensure OpenSSL is properly installed and linked
2. **Permission denied**: Make sure the executable has proper permissions
3. **SSL handshake failed**: Check network connectivity and certificate validity
4. **Library not found**: Ensure all dependencies are statically linked

### Debug SSL Issues

```cpp
// Enable SSL debugging in your code
cli.set_connection_timeout(30);
cli.set_read_timeout(30);

// Add error handling
if (!cli.is_valid()) {
    std::cerr << "SSL client initialization failed" << std::endl;
    return false;
}
```

## Performance Optimization

### 1. Static Linking
The build uses `-DANDROID_STL=c++_static` for better performance and smaller binary size.

### 2. SSL Session Caching
Consider implementing SSL session caching for repeated connections:

```cpp
// Reuse SSL client for multiple requests
static httplib::SSLClient* ssl_client = nullptr;
if (!ssl_client) {
    ssl_client = new httplib::SSLClient(host_, 443);
    ssl_client->set_connection_timeout(30);
}
```

### 3. Connection Pooling
For high-frequency requests, implement connection pooling to avoid SSL handshake overhead.

## Security Best Practices

1. **Certificate Pinning**: Implement certificate pinning for critical domains
2. **TLS Version**: Enforce minimum TLS version (1.2+)
3. **Cipher Suites**: Use strong cipher suites only
4. **Network Security**: Use Android's network security configuration

## Example SSL Configuration

```cpp
class SecureHttpClient {
private:
    httplib::SSLClient client_;
    
public:
    SecureHttpClient(const std::string& host, int port) 
        : client_(host, port) {
        // Configure SSL context
        client_.set_connection_timeout(30);
        client_.set_read_timeout(30);
        
        // Enable certificate verification
        client_.enable_server_certificate_verification(true);
        
        // Set custom CA certificate path if needed
        // client_.set_ca_cert_path("/path/to/ca/certs");
    }
    
    bool makeSecureRequest(const std::string& path, 
                          const httplib::Headers& headers,
                          std::string& response) {
        auto res = client_.Get(path, headers);
        if (res && res->status == 200) {
            response = res->body;
            return true;
        }
        return false;
    }
};
```

## Conclusion

MNNCLI can be successfully compiled for Android with full SSL/HTTPS support. The key points are:

1. Use the provided build script with proper Android NDK configuration
2. OpenSSL is already integrated and configured
3. Consider Android-specific SSL requirements (certificate store, network security)
4. Test thoroughly on actual Android devices
5. Implement proper error handling and SSL configuration

For production use, consider implementing certificate pinning and network security configurations specific to your deployment environment.
