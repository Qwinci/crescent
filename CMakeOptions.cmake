set(CONFIG_MAX_CPUS 24 CACHE STRING "Maximum number of cpus to support")

configure_file(config.hpp.in config.hpp @ONLY)
target_include_directories(crescent PRIVATE "${CMAKE_BINARY_DIR}")
