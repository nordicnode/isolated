# Unit tests
add_executable(test_basic test_basic.cpp)
target_link_libraries(test_basic PRIVATE isolated_lib)
add_test(NAME BasicTests COMMAND test_basic)

# Comprehensive benchmark
add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE isolated_lib)
