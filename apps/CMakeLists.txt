execute_process(
	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/sysroot/bin"
)

set(APPS "" CACHE INTERNAL "")

macro(APP NAME SRC)
	add_executable(${NAME} ${SRC})
	target_compile_options(${NAME} PRIVATE
		-Wall -Wextra
		-fno-exceptions -fno-rtti
		-ffreestanding -fPIE
	)
	target_include_directories(${NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
	target_link_options(${NAME} PRIVATE
		-nostdlib -Wl,--fatal-warnings
		-fPIE
	)
	target_link_libraries(${NAME} PRIVATE common)
	add_custom_command(TARGET ${NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${NAME}> "${CMAKE_BINARY_DIR}/sysroot/bin"
	)
	set(APPS "${APPS};${NAME}" CACHE INTERNAL "")
endmacro()

add_subdirectory(desktop)
