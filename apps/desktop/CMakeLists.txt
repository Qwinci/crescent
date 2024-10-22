APP(desktop
	src/main.cpp
)
target_link_libraries(desktop PRIVATE libcxx windower text)

add_subdirectory(src)
