option(MISC_TEST "Build and run runtime kernel tests" ON)

if(MISC_TEST)
    target_compile_definitions(crescent PRIVATE CONFIG_TEST)
endif()

set(MAX_CPUS 32 CACHE STRING "Maximum number of cpus supported")
target_compile_definitions(crescent PRIVATE CONFIG_MAX_CPUS=${MAX_CPUS})