project(common)

set(CMAKE_C_STANDARD 23)

add_library(common STATIC
	start.c
	sys.c
)

target_include_directories(common PUBLIC ${CMAKE_SOURCE_DIR}/kernel/include .)
target_compile_options(common PUBLIC -fno-stack-protector -Wall -Wextra)
target_link_options(common PUBLIC -nostdlib -Wl,--fatal-warnings)
