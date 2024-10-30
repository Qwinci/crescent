target_sources(crescent PRIVATE
	discovery.cpp
	spmi.cpp
	gic.cpp
	gic_v3.cpp
	psci.cpp
	timer.cpp
)

if(BOARD STREQUAL "qemu_virt")
	target_sources(crescent PRIVATE ramfb.cpp)
endif()
