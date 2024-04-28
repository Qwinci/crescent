execute_process(
	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/sysroot/lib"
)

set(LIBS "" CACHE INTERNAL "")

macro(LIB NAME SRC)
	add_library(${NAME} STATIC ${SRC})
	target_compile_options(${NAME} PRIVATE
		-Wall -Wextra
		-fno-exceptions -fno-rtti -fno-strict-aliasing -fno-stack-protector
		-ffreestanding -fPIE
	)
	target_include_directories(${NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
	target_link_options(${NAME} PRIVATE
		-nostdlib -fPIE
	)
	add_custom_command(TARGET ${NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${NAME}> "${CMAKE_BINARY_DIR}/sysroot/lib"
	)
	set(LIBS "${LIBS};${NAME}" CACHE INTERNAL "")
endmacro()

add_subdirectory(common)
add_subdirectory(libc)
add_subdirectory(libcxx)
add_subdirectory(libwindower)
