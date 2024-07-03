target_sources(crescent PRIVATE
	nic.cpp
)

if(ARCH STREQUAL "x86_64")
	target_sources(crescent PRIVATE
		rtl8168.cpp
		virtio.cpp
	)
endif()
