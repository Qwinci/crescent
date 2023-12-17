set(APPS "" CACHE INTERNAL "")

macro(App name)
	add_custom_command(TARGET ${name} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${name}> "${CMAKE_BINARY_DIR}/sysroot")
	set(APPS "${APPS};${name}" CACHE INTERNAL "")
endmacro()

add_subdirectory(user_tty)
add_subdirectory(init)
add_subdirectory(gui)
