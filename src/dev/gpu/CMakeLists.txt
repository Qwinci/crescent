if(ARCH STREQUAL "x86_64")
	target_sources(crescent PRIVATE
		bochs_vbe.cpp
	)
endif()
