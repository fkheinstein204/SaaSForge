add_executable(notification_service
    src/main.cpp
    src/notification_service.cpp
)

target_include_directories(notification_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(notification_service PRIVATE
    proto_generated
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
    target_compile_options(notification_service PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# Tests
add_executable(notification_service_test
    tests/notification_service_test.cpp
)

target_link_libraries(notification_service_test PRIVATE
    proto_generated
    common
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
)

add_test(NAME notification_service_test COMMAND notification_service_test)
