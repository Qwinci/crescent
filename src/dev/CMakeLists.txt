target_sources(crescent PRIVATE
	clock.cpp
	date_time_provider.cpp
	dev.cpp
	event.cpp
	user_dev.cpp
	random.cpp
)

if(ARCH STREQUAL "x86_64")
	target_sources(crescent PRIVATE pci.cpp)
	add_subdirectory(sound)
	add_subdirectory(gpu)
	add_subdirectory(qemu)
endif()

add_subdirectory(fb)
add_subdirectory(net)
add_subdirectory(usb)
