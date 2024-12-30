macro(APP NAME SRC)
	add_executable(${NAME} ${SRC})
	target_compile_options(${NAME} PRIVATE
		-Wall -Wextra
		-fno-exceptions -fno-rtti
		-fno-stack-protector
		-fPIE
	)
	target_link_options(${NAME} PRIVATE
		-Wl,--fatal-warnings
		-fPIE -Wl,--export-dynamic
	)
	install(TARGETS ${NAME})
endmacro()

add_subdirectory(desktop)
add_subdirectory(console)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	add_subdirectory(evm)
endif()
