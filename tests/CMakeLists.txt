add_executable(test_basic test_basic.cpp)
target_link_libraries(test_basic PRIVATE OpenMP::OpenMP_CXX Eigen3::Eigen fmt::fmt)
target_include_directories(test_basic PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME BasicTest COMMAND test_basic)
