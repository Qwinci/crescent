if(ARCH STREQUAL "x86_64")
	target_sources(crescent PRIVATE
		acpi.cpp
		events.cpp
		qacpi.cpp
		sleep.cpp
		pci.cpp
	)
endif()
