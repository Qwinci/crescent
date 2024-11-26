LIB(windower
	src/windower.cpp
)

target_include_directories(windower PUBLIC include)
target_link_libraries(windower PRIVATE common)
