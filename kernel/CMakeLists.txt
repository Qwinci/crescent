project(crescent_kernel)
include(.config/toolchain.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_C_STANDARD 23)

add_executable(crescent src/main.c)
target_include_directories(crescent PRIVATE src thirdparty include)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/.config")

target_compile_options(crescent PRIVATE
		$<$<COMPILE_LANGUAGE:C>: -Wall -Wextra
		-mgeneral-regs-only -mno-red-zone -mcmodel=kernel
		-fno-stack-protector -fno-exceptions -fno-rtti
		-fno-omit-frame-pointer
		-fPIC -nostdlibinc -Werror=switch -Werror=noderef -Werror=return-type
		-Wframe-larger-than=8192 -Werror=frame-larger-than>)
target_compile_options(crescent PRIVATE $<$<COMPILE_LANGUAGE:ASM>: -Wno-unused-command-line-argument>)
target_link_options(crescent PRIVATE -T ${PROJECT_SOURCE_DIR}/.config/kernel.ld)

include(CMakeOptions.cmake)
add_subdirectory(src)
