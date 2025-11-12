# C++ Build System Setup

## Overview
All CMakeLists.txt files have been updated with comprehensive library linking and dependency management.

## Dependencies

### System Dependencies (Already Installed)
- ✅ gRPC 1.59.0+
- ✅ Protobuf 3.x
- ✅ libpqxx 7.8.1
- ✅ GTest 1.14.0
- ✅ OpenSSL
- ✅ hiredis 1.2.0

### Additional C++ Dependencies (Need Installation)
- ❌ jwt-cpp (header-only)
- ❌ redis-plus-plus

## Installation Steps

### Option 1: Automated Installation (Recommended)
```bash
make install-cpp-deps
```

This will:
- Clone and install jwt-cpp headers to `/usr/local/include/jwt-cpp`
- Build and install redis-plus-plus library
- Update ldconfig cache

### Option 2: Manual Installation

#### jwt-cpp (header-only)
```bash
cd /tmp
git clone https://github.com/Thalhammer/jwt-cpp.git
sudo cp -r jwt-cpp/include/jwt-cpp /usr/local/include/
```

#### redis-plus-plus
```bash
cd /tmp
git clone https://github.com/sewenew/redis-plus-plus.git
cd redis-plus-plus
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

## Build Process

### Step 1: Generate Proto Stubs
```bash
make proto
```

Generates C++ and Python gRPC code in:
- `services/cpp/generated/` (C++ stubs)
- `api/generated/` (Python stubs)

### Step 2: Generate mTLS Certificates (Development)
```bash
make certs
```

Creates self-signed certificates in `certs/` directory.

### Step 3: Build C++ Services
```bash
make build-cpp
```

This will:
1. Create `services/cpp/build/` directory
2. Run CMake configuration
3. Build all 4 services + common library + proto library
4. Generate executables:
   - `auth_service` (port 50051)
   - `upload_service` (port 50052)
   - `payment_service` (port 50053)
   - `notification_service` (port 50054)

## CMakeLists.txt Structure

### Root CMakeLists.txt
**Location:** `services/cpp/CMakeLists.txt`

**Key Features:**
- Finds all required packages with fallback logic
- Handles jwt-cpp (header-only) with manual search if CONFIG fails
- Handles redis++ with manual search if CONFIG fails
- Handles libpqxx with manual search if CONFIG fails
- Includes generated proto directory
- Links OpenSSL for crypto operations

### Generated Proto Library
**Location:** `services/cpp/generated/CMakeLists.txt`

**Compiles:**
- `auth.pb.cc` + `auth.grpc.pb.cc`
- `upload.pb.cc` + `upload.grpc.pb.cc`
- `payment.pb.cc` + `payment.grpc.pb.cc`
- `notification.pb.cc` + `notification.grpc.pb.cc`

**Links:**
- gRPC::grpc++
- gRPC::grpc++_reflection
- protobuf::libprotobuf
- Threads::Threads

**Options:**
- Suppresses warnings in generated code (-w flag)

### Common Library
**Location:** `services/cpp/common/CMakeLists.txt`

**Compiles:**
- `jwt_validator.cpp` (JWT validation + Redis blacklist)
- `redis_client.cpp` (Redis operations)
- `mtls_credentials.cpp` (mTLS certificate loading)
- `tenant_context.cpp` (gRPC interceptor)
- `db_pool.cpp` (PostgreSQL connection pool)

**Links:**
- gRPC::grpc++
- jwt-cpp::jwt-cpp
- redis++::redis++
- libpqxx::pqxx
- OpenSSL::SSL
- OpenSSL::Crypto
- Threads::Threads

**Compiler Warnings:**
- Enabled: -Wall -Wextra -Wpedantic
- Disabled: -Wno-unused-parameter

### Service Libraries (All 4 Services)
**Locations:**
- `services/cpp/auth/CMakeLists.txt`
- `services/cpp/upload/CMakeLists.txt`
- `services/cpp/payment/CMakeLists.txt`
- `services/cpp/notification/CMakeLists.txt`

**Each Service Links:**
- `proto_generated` (generated gRPC code)
- `common` (shared utilities)
- gRPC::grpc++
- gRPC::grpc++_reflection
- protobuf::libprotobuf
- jwt-cpp::jwt-cpp
- redis++::redis++
- libpqxx::pqxx
- OpenSSL::SSL
- OpenSSL::Crypto
- Threads::Threads

**Each Service Test Links:**
- `proto_generated`
- `common`
- GTest::gtest
- GTest::gtest_main
- gRPC::grpc++
- protobuf::libprotobuf
- Threads::Threads

## Troubleshooting

### CMake can't find jwt-cpp
```bash
# Manual installation
sudo cp -r /path/to/jwt-cpp/include/jwt-cpp /usr/local/include/

# Or set CMAKE variable
cmake -DJWT_CPP_INCLUDE_DIR=/path/to/jwt-cpp/include ..
```

### CMake can't find redis++
```bash
# Verify installation
ldconfig -p | grep redis++

# If not found, reinstall
cd /tmp/redis-plus-plus/build
sudo make install
sudo ldconfig
```

### CMake can't find libpqxx
```bash
# Install from package manager
sudo apt-get install libpqxx-dev

# Verify
pkg-config --modversion libpqxx
```

### Build fails with "undefined reference"
This usually means a library isn't linked. Check that:
1. Library is in target_link_libraries
2. Library is installed (ldconfig -p | grep <lib>)
3. CMake found the library (check CMake output)

### Proto files not found
```bash
# Regenerate proto stubs
make proto

# Verify generation
ls -l services/cpp/generated/*.pb.cc
ls -l api/generated/*_pb2.py
```

## Compiler Requirements
- GCC 9+ or Clang 10+
- C++17 support required
- CMake 3.20+

## Library Versions
| Library | Version | Type |
|---------|---------|------|
| gRPC | 1.59.0+ | Dynamic |
| Protobuf | 3.x | Dynamic |
| jwt-cpp | Latest | Header-only |
| redis++ | Latest | Dynamic |
| libpqxx | 7.8.1+ | Dynamic |
| hiredis | 1.2.0+ | Dynamic (redis++ dependency) |
| OpenSSL | 1.1.1+ | Dynamic |
| GTest | 1.14.0+ | Dynamic |

## Next Steps

After successful build:
1. Start PostgreSQL and Redis: `docker-compose up -d postgres redis`
2. Run database migrations: `make migrate`
3. Start services individually:
   ```bash
   ./services/cpp/build/auth/auth_service &
   ./services/cpp/build/upload/upload_service &
   ./services/cpp/build/payment/payment_service &
   ./services/cpp/build/notification/notification_service &
   ```
4. Or use Docker: `docker-compose up -d`

## Testing

### Run all C++ tests
```bash
make test-cpp
```

### Run specific service tests
```bash
cd services/cpp/build
ctest -R auth_service_test -V
ctest -R upload_service_test -V
ctest -R payment_service_test -V
ctest -R notification_service_test -V
```

### Run common library tests
```bash
cd services/cpp/build
./common/common_test
```
