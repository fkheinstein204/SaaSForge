add_executable(auth_service
    src/main.cpp
    src/auth_service.cpp
)

target_include_directories(auth_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(auth_service PRIVATE
    generated_proto
    common
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    jwt-cpp::jwt-cpp
    redis++::redis++
    libpqxx::pqxx
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(auth_service PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# Tests
add_executable(auth_service_test
    tests/auth_service_test.cpp
)

target_include_directories(auth_service_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(auth_service_test PRIVATE
    generated_proto
    common
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
    protobuf::libprotobuf
    jwt-cpp::jwt-cpp
    redis++::redis++
    OpenSSL::SSL
    Threads::Threads
)

add_test(NAME auth_service_test COMMAND auth_service_test)

# API Key Scope Validation Tests
add_executable(api_key_scope_test
    tests/api_key_scope_test.cpp
)

target_link_libraries(api_key_scope_test PRIVATE
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME api_key_scope_test COMMAND api_key_scope_test)

# Integration Tests (require PostgreSQL and Redis)
add_executable(auth_integration_test
    tests/auth_integration_test.cpp
    src/auth_service.cpp
)

target_include_directories(auth_integration_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(auth_integration_test PRIVATE
    generated_proto
    common
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
    protobuf::libprotobuf
    jwt-cpp::jwt-cpp
    redis++::redis++
    libpqxx::pqxx
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# Note: Integration tests are run separately via run-integration-tests.sh
# They require test containers to be running
add_test(NAME auth_integration_test COMMAND auth_integration_test)
set_tests_properties(auth_integration_test PROPERTIES
    LABELS "integration"
    ENVIRONMENT "SAASFORGE_TEST_MODE=1"
)
