execute_process(
	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/sysroot/bin"
)

set(APPS "" CACHE INTERNAL "")

macro(APP NAME SRC)
	add_executable(${NAME} ${SRC})
	target_compile_options(${NAME} PRIVATE
		--sysroot "${CMAKE_BINARY_DIR}/sysroot"
		-Wall -Wextra -fno-stack-protector
		-fno-exceptions -fno-rtti -nostdinc++
		-fPIE
	)
	target_include_directories(${NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
	target_link_options(${NAME} PRIVATE
		--sysroot "${CMAKE_BINARY_DIR}/sysroot"
		-Wl,--fatal-warnings
		-fPIE -Wl,--export-dynamic -nostdlib++
	)
	add_dependencies(${NAME} hzlibc)
	add_custom_command(TARGET ${NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${NAME}> "${CMAKE_BINARY_DIR}/sysroot/bin"
	)
	set(APPS "${APPS};${NAME}" CACHE INTERNAL "")
endmacro()

add_subdirectory(desktop)
add_subdirectory(console)
