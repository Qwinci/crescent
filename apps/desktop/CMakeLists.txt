APP(desktop
	src/main.cpp
)
target_link_libraries(desktop PRIVATE windower ui common)

add_subdirectory(src)
