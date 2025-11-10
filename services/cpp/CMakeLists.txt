cmake_minimum_required(VERSION 3.20)
project(SaaSForge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(redis++ REQUIRED)
find_package(GTest REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Build subdirectories
add_subdirectory(common)
add_subdirectory(auth)
add_subdirectory(upload)
add_subdirectory(payment)
add_subdirectory(notification)

# Enable testing
enable_testing()
