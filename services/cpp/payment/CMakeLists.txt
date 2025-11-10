add_executable(payment_service
    src/main.cpp
    src/payment_service.cpp
)

target_include_directories(payment_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(payment_service PRIVATE
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    jwt-cpp::jwt-cpp
    redis++::redis++
    common
    Threads::Threads
)

# Tests
add_executable(payment_service_test
    tests/payment_service_test.cpp
)

target_link_libraries(payment_service_test PRIVATE
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
)

add_test(NAME payment_service_test COMMAND payment_service_test)
