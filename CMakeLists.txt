cmake_minimum_required(VERSION 3.27)
project(crescent CXX ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)

if(BUILD_APPS)
	add_subdirectory(libs)
	add_subdirectory(apps)
	if(NOT BUILD_EVERYTHING)
		return()
	endif()
endif()

set(ARCH "x86_64" CACHE STRING "Architecture to compile for")

include(qacpi/lib.cmake)

add_executable(crescent
	src/main.cpp
)
target_include_directories(crescent PRIVATE
	src
	src/std
	include
)

target_compile_options(crescent PRIVATE
	-Wall -Wextra -Werror=switch -Werror=return-type -Werror=unused-result
	-nostdinc++ -fno-exceptions -fno-threadsafe-statics -fno-rtti -fsized-deallocation
	-ffreestanding -fno-strict-aliasing -Wframe-larger-than=4096 -fno-eliminate-unused-debug-types
	-Wno-invalid-offsetof -Wno-unused-parameter -Wno-unused-const-variable
)

if(NOT ARCH STREQUAL "user")
	target_compile_options(crescent PRIVATE
		-mgeneral-regs-only -fno-stack-protector
		-fno-omit-frame-pointer -mno-red-zone
	)
	target_link_options(crescent PRIVATE
		-static -nostdlib -Wl,--fatal-warnings
		-Wl,--export-dynamic -Wl,--build-id=none
	)
endif()

add_subdirectory(src)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(crescent PRIVATE -Wno-unused-command-line-argument)
endif()

if(ARCH STREQUAL "x86_64")
	target_compile_options(crescent PRIVATE -fPIE)
	target_compile_definitions(crescent PRIVATE ARCH_X86_64)
	target_link_options(crescent PRIVATE -T ${PROJECT_SOURCE_DIR}/lds/x86_64.ld -static-pie)

	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		target_compile_options(crescent PRIVATE -fsanitize=undefined -fsanitize-minimal-runtime)
	endif()

	target_link_libraries(crescent PRIVATE qacpi::qacpi qacpi::events)
elseif(ARCH STREQUAL "aarch64")
	target_compile_options(crescent PRIVATE -fPIE)
	target_link_options(crescent PRIVATE -T ${PROJECT_SOURCE_DIR}/lds/aarch64.ld -static-pie)
	target_compile_definitions(crescent PRIVATE ARCH_AARCH64)

	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		target_compile_options(crescent PRIVATE -fsanitize=undefined -fsanitize-minimal-runtime)
	endif()

	include(cmake/image_aarch64.cmake)
elseif(ARCH STREQUAL "user")
	target_compile_options(crescent PRIVATE -fPIE -fsanitize=undefined,address)
	target_link_options(crescent PRIVATE -fsanitize=undefined,address)
	target_compile_definitions(crescent PRIVATE ARCH_USER)

	include(cmake/image_user.cmake)
else()
	message(FATAL_ERROR "Unsupported architecture ${ARCH}")
endif()

include(CMakeOptions.cmake)

target_sources(crescent PUBLIC FILE_SET HEADERS
	BASE_DIRS include
	FILES

	include/crescent/aarch64/arch_syscall.h
	include/crescent/aarch64/posix_arch_syscall.h
	include/crescent/x86/arch_syscall.h
	include/crescent/x86/posix_arch_syscall.h
	include/crescent/devlink.h
	include/crescent/event.h
	include/crescent/evm.h
	include/crescent/socket.h
	include/crescent/syscall.h
	include/crescent/syscalls.h
	include/crescent/posix_syscall.h
	include/crescent/posix_syscalls.h
	include/crescent/time.h
)

install(FILES limine.conf fonts/Tamsyn8x16r.psf DESTINATION "crescent")
install(TARGETS crescent FILE_SET HEADERS
	RUNTIME DESTINATION "crescent"
)
