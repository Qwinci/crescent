include(${CMAKE_CURRENT_LIST_DIR}/toolchain_clang_aarch64.cmake)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8.2-a")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8.2-a")

set(BOARD "msm")
