add_library(common STATIC
    src/jwt_validator.cpp
    src/redis_client.cpp
    src/mtls_credentials.cpp
    src/tenant_context.cpp
    src/db_pool.cpp
    src/password_hasher.cpp
    src/totp_helper.cpp
    src/mock_stripe_client.cpp
    src/email_queue.cpp
    src/webhook_delivery.cpp
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

# TOTP helper tests
add_executable(totp_helper_test
    tests/totp_helper_test.cpp
)

target_link_libraries(totp_helper_test PRIVATE
    common
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME totp_helper_test COMMAND totp_helper_test)

# Mock Stripe client tests
add_executable(mock_stripe_client_test
    tests/mock_stripe_client_test.cpp
)

target_link_libraries(mock_stripe_client_test PRIVATE
    common
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME mock_stripe_client_test COMMAND mock_stripe_client_test)

# Webhook SSRF protection tests
add_executable(webhook_ssrf_test
    tests/webhook_ssrf_test.cpp
)

target_link_libraries(webhook_ssrf_test PRIVATE
    common
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME webhook_ssrf_test COMMAND webhook_ssrf_test)

# Email queue tests
add_executable(email_queue_test
    tests/email_queue_test.cpp
)

target_link_libraries(email_queue_test PRIVATE
    common
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME email_queue_test COMMAND email_queue_test)
