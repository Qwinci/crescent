include(archdetect)
include(CheckLinkerFlag)
target_arch(TARGET_ARCH)

include(CMakeOptions.cmake)

if(CONFIG_TARGET STREQUAL "x86_64")
	include(toolchain_x86_64)
	target_compile_definitions(crescent PRIVATE TARGET=x86_64 WORDSIZE=64)
else()
message(FATAL_ERROR "Unsupported target ${CONFIG_TARGET}")
endif()

option(CONFIG_USE_LLD "Use the lld linker" ON)

CHECK_LINKER_FLAG(CXX -fuse-ld=lld LINKER_SUPPORTS_FUSE_LD_LLD)
if(CONFIG_USE_LLD)
	if(LINKER_SUPPORTS_FUSE_LD_LLD)
		target_link_options(crescent PRIVATE -fuse-ld=lld)
	else()
		message(FATAL_ERROR "Linker does not support lld, set CONFIG_USE_LLD to OFF")
	endif()
endif()