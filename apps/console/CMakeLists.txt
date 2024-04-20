APP(console
	src/main.cpp
)
target_link_libraries(console PRIVATE libcxx)

add_subdirectory(src)
