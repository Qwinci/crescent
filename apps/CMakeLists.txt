set(APPS
	user_tty
	init
	gui
)

add_subdirectory(user_tty)
add_subdirectory(init)
add_subdirectory(gui)

set(APP_FILES ${APPS})
list(TRANSFORM APP_FILES PREPEND "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
add_custom_target(copy_apps
	COMMAND cp ${APP_FILES} ${CMAKE_BINARY_DIR}/sysroot
	USES_TERMINAL VERBATIM
)
add_dependencies(copy_apps ${APPS})
