find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(snowglobe_tests
    smoke_test.cpp)

target_link_libraries(snowglobe_tests PRIVATE
    snowglobe_core
    snowglobe_project_options
    GTest::gtest
    GTest::gtest_main)

# Register each TEST() with CTest so `ctest --preset <name>` runs them.
gtest_discover_tests(snowglobe_tests)
