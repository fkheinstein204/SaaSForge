add_executable(payment_service
    src/main.cpp
    src/payment_service.cpp
)

target_include_directories(payment_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(payment_service PRIVATE
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
    target_compile_options(payment_service PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# Tests
add_executable(payment_service_test
    tests/payment_service_test.cpp
)

target_include_directories(payment_service_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(payment_service_test PRIVATE
    generated_proto
    common
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
)

add_test(NAME payment_service_test COMMAND payment_service_test)
