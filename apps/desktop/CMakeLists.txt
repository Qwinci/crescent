APP(desktop
	src/main.cpp
)
target_link_libraries(desktop PRIVATE libcxx)

add_subdirectory(src)
