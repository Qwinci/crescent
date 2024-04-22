APP(console
	src/main.cpp
)
target_link_libraries(console PRIVATE libcxx windower)

add_subdirectory(src)
