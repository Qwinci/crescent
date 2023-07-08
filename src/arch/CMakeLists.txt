target_sources(crescent PRIVATE map.cpp)

if(CONFIG_TARGET STREQUAL "x86_64")
	add_subdirectory(x86)
endif()