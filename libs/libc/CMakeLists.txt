LIB(libc
	src/stdio.cpp
)
target_sources(libc PRIVATE
	src/string.cpp
	src/stdlib.cpp
	src/cxx.cpp
)

target_include_directories(libc SYSTEM PUBLIC include)
target_include_directories(libc SYSTEM PRIVATE ../libcxx/include)
target_link_libraries(libc PRIVATE common)
