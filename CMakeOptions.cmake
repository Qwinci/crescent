option(MISC_TEST "Build and run runtime kernel tests" ON)

if(MISC_TEST)
    target_compile_definitions(crescent PRIVATE CONFIG_TEST)
endif()