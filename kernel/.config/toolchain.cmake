include(archdetect)
target_arch(ARCH)

if(ARCH STREQUAL "x86_64")
	include(toolchain_x86_64)
else()
	message(FATAL_ERROR "Unsupported target architecture ${ARCH}")
endif()
