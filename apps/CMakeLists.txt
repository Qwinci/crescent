set(APPS
	user_tty
	init
)

add_subdirectory(user_tty)
add_subdirectory(init)

set(APP_FILES ${APPS})
list(TRANSFORM APP_FILES PREPEND "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
add_custom_target(copy_apps
	DEPENDS ${APPS}
	COMMAND cp ${APP_FILES} ${CMAKE_BINARY_DIR}/sysroot
	USES_TERMINAL VERBATIM
)
