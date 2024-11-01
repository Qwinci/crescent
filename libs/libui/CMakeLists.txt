LIB(ui
	src/context.cpp
	src/gui.cpp
	src/window.cpp
	src/button.cpp
	src/text.cpp
)

target_include_directories(ui PUBLIC include)
target_link_libraries(ui PUBLIC libcxx text)
target_link_libraries(ui PRIVATE common)
