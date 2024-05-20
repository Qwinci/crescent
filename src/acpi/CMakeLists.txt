if(ARCH STREQUAL "x86_64")
	target_sources(crescent PRIVATE
		acpi.cpp
		qacpi.cpp
		sleep.cpp
		pci.cpp
	)
endif()
