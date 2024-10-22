LIB(libc
	src/stdio.cpp
	src/string.cpp
	src/stdlib.cpp
	src/cxx.cpp
	src/assert.cpp
)
set_source_files_properties(src/string.cpp PROPERTIES COMPILE_FLAGS -O2)

add_subdirectory(hzutils)

target_include_directories(libc SYSTEM PUBLIC include)
target_include_directories(libc SYSTEM PRIVATE ../libcxx/include)
target_link_libraries(libc PRIVATE common hzutils)
