set(APPS
	user_tty
)
set(APP_FILES
	user_tty/user_tty
)

add_subdirectory(user_tty)

add_custom_target(copy_apps
	DEPENDS ${APPS}
	COMMAND cp ${APP_FILES} ${CMAKE_BINARY_DIR}/sysroot
	USES_TERMINAL VERBATIM
)
