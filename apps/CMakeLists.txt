execute_process(
	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/sysroot/bin"
)

set(APPS "" CACHE INTERNAL "")

execute_process(COMMAND
	${CMAKE_C_COMPILER} -print-search-dirs
	OUTPUT_VARIABLE COMPILER_DIRS
)
string(REPLACE "\n" ";" COMPILER_DIRS "${COMPILER_DIRS}")
foreach(LINE IN LISTS COMPILER_DIRS)
	if(LINE MATCHES "^libraries: =")
		string(SUBSTRING "${LINE}" 12 -1 LINE)
		string(REPLACE ":" ";" LINE "${LINE}")
		list(GET LINE 0 COMPILER_DIR)
		break()
	endif()
endforeach()

if(NOT COMPILER_DIR)
	message(FATAL_ERROR "Failed to find compiler directory")
endif()

macro(APP NAME SRC)
	add_executable(${NAME} ${SRC})
	target_compile_options(${NAME} PRIVATE
		-Wall -Wextra
		-fno-exceptions -fno-rtti -fno-stack-protector
		-ffreestanding -fPIE -nostdinc
		-I "${COMPILER_DIR}/include"
	)
	target_include_directories(${NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
	target_link_options(${NAME} PRIVATE
		-nostdlib -Wl,--fatal-warnings
		-fPIE -Wl,--export-dynamic
	)
	target_link_libraries(${NAME} PRIVATE common libc)
	add_custom_command(TARGET ${NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${NAME}> "${CMAKE_BINARY_DIR}/sysroot/bin"
	)
	set(APPS "${APPS};${NAME}" CACHE INTERNAL "")
endmacro()

add_subdirectory(desktop)
add_subdirectory(console)
