cmake_minimum_required(VERSION 3.20)
project(SaaSForge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set module path for custom Find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find required packages
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# jwt-cpp is header-only, try CONFIG first, then fallback to manual
find_package(jwt-cpp CONFIG QUIET)
if(NOT jwt-cpp_FOUND)
    message(STATUS "jwt-cpp not found via CONFIG, looking for headers manually")
    find_path(JWT_CPP_INCLUDE_DIR jwt-cpp/jwt.h
        PATHS
        ${CMAKE_SOURCE_DIR}/../../third_party/jwt-cpp/include
        ${CMAKE_SOURCE_DIR}/../third_party/jwt-cpp/include
        /usr/local/include
        /usr/include
    )
    if(JWT_CPP_INCLUDE_DIR)
        add_library(jwt-cpp::jwt-cpp INTERFACE IMPORTED)
        set_target_properties(jwt-cpp::jwt-cpp PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${JWT_CPP_INCLUDE_DIR}"
        )
        message(STATUS "Found jwt-cpp headers at: ${JWT_CPP_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "jwt-cpp not found. Please install jwt-cpp or set JWT_CPP_INCLUDE_DIR")
    endif()
endif()

# redis++ (redis-plus-plus)
find_package(redis++ CONFIG QUIET)
if(NOT redis++_FOUND)
    message(STATUS "redis++ not found via CONFIG, looking for library manually")
    find_library(REDIS_PLUS_PLUS_LIB NAMES redis++ redis++-static
        PATHS
        ${CMAKE_SOURCE_DIR}/../../third_party/lib
        ${CMAKE_SOURCE_DIR}/../third_party/lib
        /usr/local/lib
        /usr/lib
        /usr/lib/aarch64-linux-gnu
        /usr/lib/x86_64-linux-gnu
    )
    find_path(REDIS_PLUS_PLUS_INCLUDE_DIR sw/redis++/redis++.h
        PATHS
        ${CMAKE_SOURCE_DIR}/../../third_party/include
        ${CMAKE_SOURCE_DIR}/../third_party/include
        /usr/local/include
        /usr/include
    )
    if(REDIS_PLUS_PLUS_LIB AND REDIS_PLUS_PLUS_INCLUDE_DIR)
        add_library(redis++::redis++ SHARED IMPORTED)
        set_target_properties(redis++::redis++ PROPERTIES
            IMPORTED_LOCATION "${REDIS_PLUS_PLUS_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${REDIS_PLUS_PLUS_INCLUDE_DIR}"
        )
        # redis++ depends on hiredis
        find_library(HIREDIS_LIB NAMES hiredis)
        if(HIREDIS_LIB)
            set_property(TARGET redis++::redis++ PROPERTY
                INTERFACE_LINK_LIBRARIES "${HIREDIS_LIB}"
            )
        endif()
        message(STATUS "Found redis++: ${REDIS_PLUS_PLUS_LIB}")
    else()
        message(FATAL_ERROR "redis++ not found. Please install redis-plus-plus")
    endif()
endif()

# libpqxx (PostgreSQL C++ client)
find_package(libpqxx CONFIG QUIET)
if(NOT libpqxx_FOUND)
    message(STATUS "libpqxx not found via CONFIG, looking for library manually")
    find_library(PQXX_LIB NAMES pqxx
        PATHS
        /usr/local/lib
        /usr/lib
        /usr/lib/aarch64-linux-gnu
        /usr/lib/x86_64-linux-gnu
    )
    find_path(PQXX_INCLUDE_DIR pqxx/pqxx
        PATHS
        /usr/local/include
        /usr/include
    )
    if(PQXX_LIB AND PQXX_INCLUDE_DIR)
        add_library(libpqxx::pqxx SHARED IMPORTED)
        set_target_properties(libpqxx::pqxx PROPERTIES
            IMPORTED_LOCATION "${PQXX_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${PQXX_INCLUDE_DIR}"
        )
        # pqxx depends on pq (PostgreSQL client library)
        find_library(PQ_LIB NAMES pq)
        if(PQ_LIB)
            set_property(TARGET libpqxx::pqxx PROPERTY
                INTERFACE_LINK_LIBRARIES "${PQ_LIB}"
            )
        endif()
        message(STATUS "Found libpqxx: ${PQXX_LIB}")
    else()
        message(FATAL_ERROR "libpqxx not found. Please install libpqxx-dev")
    endif()
endif()

# GTest
find_package(GTest REQUIRED)

# OpenSSL (required by gRPC and jwt-cpp)
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/generated)
include_directories(${OPENSSL_INCLUDE_DIR})

# Build subdirectories
add_subdirectory(generated)
add_subdirectory(common)
add_subdirectory(auth)
add_subdirectory(upload)
add_subdirectory(payment)
add_subdirectory(notification)

# Enable testing
enable_testing()
