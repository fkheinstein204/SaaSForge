add_executable(notification_service
    src/main.cpp
    src/notification_service.cpp
)

target_include_directories(notification_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(notification_service PRIVATE
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    jwt-cpp::jwt-cpp
    redis++::redis++
    common
    Threads::Threads
)

# Tests
add_executable(notification_service_test
    tests/notification_service_test.cpp
)

target_link_libraries(notification_service_test PRIVATE
    GTest::gtest
    GTest::gtest_main
    gRPC::grpc++
)

add_test(NAME notification_service_test COMMAND notification_service_test)
