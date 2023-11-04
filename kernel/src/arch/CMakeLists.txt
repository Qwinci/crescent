message(NOTICE "Compiling for architecture ${ARCH}")

if(ARCH STREQUAL "x86_64")
    add_subdirectory(x86)
else()
    message(FATAL_ERROR "unsupported architecture")
endif()