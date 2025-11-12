add_library(common STATIC
    src/jwt_validator.cpp
    src/redis_client.cpp
    src/mtls_credentials.cpp
    src/tenant_context.cpp
    src/db_pool.cpp
    src/password_hasher.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(common PUBLIC
    gRPC::grpc++
    jwt-cpp::jwt-cpp
    redis++::redis++
    libpqxx::pqxx
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    argon2  # Password hashing library
)

# Enable warnings for our code
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(common PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# Common tests
add_executable(common_test
    tests/jwt_validator_test.cpp
    tests/redis_client_test.cpp
)

target_link_libraries(common_test PRIVATE
    common
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME common_test COMMAND common_test)
