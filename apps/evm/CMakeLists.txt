APP(evm
	src/main.cpp
)
target_link_libraries(evm PRIVATE windower common)

add_subdirectory(src)
