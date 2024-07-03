LIB(libc
	src/stdio.cpp
)
target_sources(libc PRIVATE
	src/string.cpp
	src/stdlib.cpp
	src/cxx.cpp
)
set_source_files_properties(src/string.cpp PROPERTIES COMPILE_FLAGS -O2)

target_include_directories(libc SYSTEM PUBLIC include)
target_include_directories(libc SYSTEM PRIVATE ../libcxx/include)
target_link_libraries(libc PRIVATE common)
