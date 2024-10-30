if(ARCH STREQUAL "x86_64")
	target_sources(crescent PRIVATE
		fw_cfg_x86.cpp
	)
elseif(ARCH STREQUAL "aarch64")
	target_sources(crescent PRIVATE
		fw_cfg_aarch64.cpp
	)
endif()
