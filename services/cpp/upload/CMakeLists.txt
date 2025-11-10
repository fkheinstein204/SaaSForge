add_executable(upload_service
    src/main.cpp
    src/upload_service.cpp
)

target_include_directories(upload_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(upload_service PRIVATE
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    jwt-cpp::jwt-cpp
    redis++::redis++
    common
    Threads::Threads
)

# Tests
add_executable(upload_service_test
    tests/upload_service_test.cpp
)

target_link_libraries(upload_service_test PRIVATE
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
)

add_test(NAME upload_service_test COMMAND upload_service_test)
