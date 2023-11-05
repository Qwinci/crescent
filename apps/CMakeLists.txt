set(APPS
	user_tty
	libc_test_app
)
set(APP_FILES
	user_tty/user_tty
	libc_test_app/libc_test_app
)

add_subdirectory(user_tty)
add_subdirectory(libc_test_app)

add_custom_target(copy_apps
	DEPENDS ${APPS}
	COMMAND cp ${APP_FILES} ${CMAKE_BINARY_DIR}/sysroot
	USES_TERMINAL VERBATIM
)
