cmake_minimum_required(VERSION 3.16)
project(rvsim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(rvcore src/cpu.cpp src/disasm.cpp src/loader.cpp)
target_include_directories(rvcore PUBLIC src)
target_compile_options(rvcore PRIVATE -Wall -Wextra)

add_executable(rvsim src/main.cpp)
target_link_libraries(rvsim rvcore)

enable_testing()
add_executable(test_cpu tests/test_cpu.cpp)
target_link_libraries(test_cpu rvcore)

# En Windows/MinGW, enlazar estático para que el .exe no dependa de las DLL de MSYS2.
if(MINGW)
  target_link_options(rvsim PRIVATE -static)
  target_link_options(test_cpu PRIVATE -static)
endif()
add_test(NAME test_cpu COMMAND test_cpu)
