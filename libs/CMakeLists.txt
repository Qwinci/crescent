execute_process(
	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/sysroot/usr/lib"
	COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/sysroot/usr/lib/libm.a"
)

set(LIBS "" CACHE INTERNAL "")

macro(LIB NAME)
	add_library(${NAME} STATIC ${ARGN})
	target_compile_options(${NAME} PRIVATE
		--sysroot "${CMAKE_BINARY_DIR}/sysroot"
		-Wall -Wextra -fno-stack-protector
		-fno-exceptions -fno-rtti -fno-strict-aliasing -nostdinc++
		-fPIC
	)
	target_include_directories(${NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
	target_link_options(${NAME} PRIVATE
		--sysroot "${CMAKE_BINARY_DIR}/sysroot"
		-fPIC -nostdlib++
	)
	add_dependencies(${NAME} hzlibc)
	add_custom_command(TARGET ${NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${NAME}> "${CMAKE_BINARY_DIR}/sysroot/usr/lib"
	)
	set(LIBS "${LIBS};${NAME}" CACHE INTERNAL "")
endmacro()

macro(SHARED_LIB NAME)
	add_library(${NAME} SHARED ${ARGN})
	target_compile_options(${NAME} PRIVATE
		--sysroot "${CMAKE_BINARY_DIR}/sysroot"
		-Wall -Wextra -fno-stack-protector
		-fno-exceptions -fno-rtti -fno-strict-aliasing -nostdinc++
		-fPIC
	)
	target_include_directories(${NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
	target_link_options(${NAME} PRIVATE
		--sysroot "${CMAKE_BINARY_DIR}/sysroot"
		-fPIC -nostdlib++
	)
	add_dependencies(${NAME} hzlibc)
	add_custom_command(TARGET ${NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${NAME}> "${CMAKE_BINARY_DIR}/sysroot/usr/lib"
	)
	set(LIBS "${LIBS};${NAME}" CACHE INTERNAL "")
endmacro()

add_subdirectory(common)
add_subdirectory(libcxx)
add_subdirectory(libnet)
add_subdirectory(libtext)
add_subdirectory(libwindower)

include(ExternalProject)

ExternalProject_Add(
	hzlibc_external
	GIT_REPOSITORY "https://github.com/Qwinci/hzlibc.git"
	GIT_TAG main
	GIT_REMOTE_UPDATE_STRATEGY REBASE
	PATCH_COMMAND
	${CMAKE_COMMAND} -E make_directory "subprojects"
	COMMAND
	${CMAKE_COMMAND} -E create_symlink "${CMAKE_SOURCE_DIR}" "subprojects/crescent"
	CMAKE_ARGS
	-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
	-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
	-DCMAKE_C_COMPILER_TARGET=${CMAKE_C_COMPILER_TARGET}
	-DCMAKE_CXX_COMPILER_TARGET=${CMAKE_CXX_COMPILER_TARGET}
	-DCMAKE_ASM_COMPILER_TARGET=${CMAKE_ASM_COMPILER_TARGET}
	-DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
	-DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}
	-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/sysroot/usr
	-DCMAKE_SYSTEM_NAME=Crescent
	-DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
	-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
	-DANSI_ONLY=ON
)

add_library(hzlibc INTERFACE)
add_dependencies(hzlibc hzlibc_external)

find_program(PATCHELF patchelf REQUIRED)

add_library(crtbegin OBJECT libcxx/src/dummy.cpp)
target_compile_options(crtbegin PRIVATE -fno-exceptions -fno-rtti -fPIC)
add_library(crtend OBJECT libcxx/src/dummy.cpp)
target_compile_options(crtend PRIVATE -fno-exceptions -fno-rtti -fPIC)

add_library(gcc_s SHARED libcxx/src/dummy.cpp)
add_dependencies(gcc_s crtbegin crtend)
target_compile_options(gcc_s PRIVATE
	--sysroot "${CMAKE_BINARY_DIR}/sysroot"
	-Wall -Wextra
	-fno-exceptions -fno-rtti -fno-strict-aliasing
	-fPIC
)
target_link_options(gcc_s PRIVATE
	--sysroot "${CMAKE_BINARY_DIR}/sysroot"
	-fPIC -nostdlib
)
add_custom_command(TARGET gcc_s POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:gcc_s> "${CMAKE_BINARY_DIR}/sysroot/usr/lib/libgcc_s.so.1"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_OBJECTS:crtbegin> "${CMAKE_BINARY_DIR}/sysroot/usr/lib/crtbeginS.o"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_OBJECTS:crtend> "${CMAKE_BINARY_DIR}/sysroot/usr/lib/crtendS.o"
	COMMAND ${PATCHELF} --set-soname libgcc_s.so.1 ${CMAKE_BINARY_DIR}/sysroot/usr/lib/libgcc_s.so.1
	COMMAND ${CMAKE_COMMAND} -E create_symlink libgcc_s.so.1 "${CMAKE_BINARY_DIR}/sysroot/usr/lib/libgcc_s.so"
	COMMAND ${CMAKE_COMMAND} -E create_symlink libgcc_s.so.1 "${CMAKE_BINARY_DIR}/sysroot/usr/lib/libgcc.so"
)
set(LIBS "${LIBS};gcc_s" CACHE INTERNAL "")

add_dependencies(hzlibc gcc_s)
