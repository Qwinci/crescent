target_sources(crescent PRIVATE
	loader/start.S
	loader/start.cpp

	mem/std_mem.cpp
	mem/paging.cpp
	mem/user.S

	start.cpp
	smp.cpp
	arch_sched.cpp
	mod.cpp
	smp_bringup.cpp

	acpi/madt.cpp
)
target_include_directories(crescent PRIVATE include)

add_subdirectory(dev)
add_subdirectory(interrupts)
