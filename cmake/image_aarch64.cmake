add_custom_command(TARGET crescent POST_BUILD
	COMMAND ${CMAKE_OBJCOPY} -O binary bin/crescent crescent.bin
	DEPENDS crescent
)

install(FILES "${CMAKE_BINARY_DIR}/crescent.bin"
	DESTINATION "crescent"
)
