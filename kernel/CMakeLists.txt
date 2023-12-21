cmake_minimum_required(VERSION 3.19)
project(crescent C ASM)
include(CheckCCompilerFlag)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_C_STANDARD 23)

add_executable(crescent src/main.c)
target_include_directories(crescent PRIVATE src thirdparty include)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/.config")
include(.config/toolchain.cmake)

target_compile_options(crescent PRIVATE
		$<$<COMPILE_LANGUAGE:C>: -Wall -Wextra
		-mgeneral-regs-only
		-fno-stack-protector
		-fno-omit-frame-pointer
		-fPIE -Werror=switch -Werror=return-type>)
target_link_options(crescent PRIVATE -static -static-pie -nostdlib -Wl,--fatal-warnings)

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
	target_compile_options(crescent PRIVATE $<$<COMPILE_LANGUAGE:C>: -Wframe-larger-than=8192 -Werror=frame-larger-than>)
else()
	target_compile_options(crescent PRIVATE $<$<COMPILE_LANGUAGE:C>: -Werror=frame-larger-than=8192>)
endif()

check_c_compiler_flag(-nostdlibinc COMPILER_SUPPORTS_NOSTDLIBINC)
check_c_compiler_flag(-Werror=noderef COMPILER_SUPPORTS_NODEREF)
if(COMPILER_SUPPORTS_NOSTDLIBINC)
	target_compile_options(crescent PRIVATE $<$<COMPILE_LANGUAGE:C>: -nostdlibinc>)
endif()
if(COMPILER_SUPPORTS_NODEREF)
	target_compile_options(crescent PRIVATE $<$<COMPILE_LANGUAGE:C>: -Werror=noderef>)
endif()

target_compile_options(crescent PRIVATE $<$<COMPILE_LANGUAGE:ASM>: -Wno-unused-command-line-argument>)

include(CMakeOptions.cmake)
add_subdirectory(src)

install(TARGETS crescent DESTINATION "crescent")
install(FILES fonts/Tamsyn8x16r.psf limine.cfg DESTINATION "crescent")
install(DIRECTORY "include/crescent" DESTINATION "include")
