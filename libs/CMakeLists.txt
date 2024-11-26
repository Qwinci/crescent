macro(LIB NAME)
	add_library(${NAME} STATIC ${ARGN})
	target_compile_options(${NAME} PRIVATE
		-Wall -Wextra
		-fno-exceptions -fno-rtti
		-fno-strict-aliasing -fno-stack-protector
		-fPIC
	)
endmacro()

add_subdirectory(common)
add_subdirectory(libnet)
add_subdirectory(libtext)
add_subdirectory(libui)
add_subdirectory(libwindower)
