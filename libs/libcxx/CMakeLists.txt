LIB(libcxx
	src/dummy.cpp
)

target_include_directories(libcxx SYSTEM PUBLIC include)
target_link_libraries(libcxx PRIVATE libc)
