set(BOARD "qemu_virt" CACHE STRING "Board to target")
set_property(CACHE BOARD PROPERTY STRINGS qemu_virt msm)

if(BOARD STREQUAL "qemu_virt")
	target_compile_definitions(crescent PRIVATE BOARD_QEMU_VIRT)
elseif(BOARD STREQUAL "msm")
	target_compile_definitions(crescent PRIVATE BOARD_MSM)
endif()

target_sources(crescent PRIVATE
	loader/start.S
	loader/start.cpp
	loader/early_paging.cpp
	mem/std_mem.cpp
	mem/paging.cpp
	mem/mem.cpp
	mem/user.S

	kernel_dtb.cpp
	dtb.cpp
	arch_sched.cpp
	cpu.cpp
	smp.cpp
	start.cpp
)
target_include_directories(crescent PRIVATE include)

add_subdirectory(dev)
add_subdirectory(interrupts)
