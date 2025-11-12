#!/bin/bash
# Install C++ dependencies for SaaSForge

set -e

echo "Installing C++ dependencies..."

# Install to project third_party directory
INSTALL_DIR="${PWD}/third_party"
mkdir -p "$INSTALL_DIR"

# Install jwt-cpp (header-only library)
echo "Installing jwt-cpp..."
if [ ! -d "$INSTALL_DIR/jwt-cpp" ]; then
    cd /tmp
    if [ -d "jwt-cpp" ]; then
        rm -rf jwt-cpp
    fi
    git clone --depth 1 https://github.com/Thalhammer/jwt-cpp.git
    mkdir -p "$INSTALL_DIR/jwt-cpp"
    cp -r jwt-cpp/include "$INSTALL_DIR/jwt-cpp/"
    echo "jwt-cpp installed to $INSTALL_DIR/jwt-cpp"
    cd /tmp
    rm -rf jwt-cpp
else
    echo "jwt-cpp already installed in third_party"
fi

# Install redis-plus-plus
echo "Installing redis-plus-plus..."
if [ ! -f "$INSTALL_DIR/lib/libredis++.so" ] && [ ! -f "$INSTALL_DIR/lib/libredis++.a" ]; then
    cd /tmp
    if [ -d "redis-plus-plus" ]; then
        rm -rf redis-plus-plus
    fi
    git clone --depth 1 https://github.com/sewenew/redis-plus-plus.git
    cd redis-plus-plus
    mkdir -p build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
          -DREDIS_PLUS_PLUS_BUILD_TEST=OFF ..
    make -j$(nproc)
    make install
    echo "redis-plus-plus installed to $INSTALL_DIR"
    cd /tmp
    rm -rf redis-plus-plus
else
    echo "redis-plus-plus already installed in third_party"
fi

echo "All C++ dependencies installed successfully!"
echo ""
echo "Installed libraries:"
echo "  - jwt-cpp (header-only)"
echo "  - redis-plus-plus"
echo ""
echo "System dependencies already available:"
echo "  - gRPC"
echo "  - Protobuf"
echo "  - libpqxx"
echo "  - GTest"
echo "  - OpenSSL"
