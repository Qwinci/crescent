APP(console
	src/main.cpp
)
target_link_libraries(console PRIVATE windower net common)

add_subdirectory(src)
