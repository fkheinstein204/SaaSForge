add_executable(auth_service
    src/main.cpp
    src/auth_service.cpp
)

target_include_directories(auth_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(auth_service PRIVATE
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    jwt-cpp::jwt-cpp
    redis++::redis++
    common
    Threads::Threads
)

# Tests
add_executable(auth_service_test
    tests/auth_service_test.cpp
)

target_link_libraries(auth_service_test PRIVATE
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
    jwt-cpp::jwt-cpp
)

add_test(NAME auth_service_test COMMAND auth_service_test)
