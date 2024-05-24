target_sources(crescent PRIVATE
	clock.cpp
	event.cpp
	dev.cpp
	random.cpp
)

if(ARCH STREQUAL "x86_64")
	target_sources(crescent PRIVATE pci.cpp)
	add_subdirectory(net)
	add_subdirectory(sound)
	add_subdirectory(usb)
endif()

add_subdirectory(fb)
