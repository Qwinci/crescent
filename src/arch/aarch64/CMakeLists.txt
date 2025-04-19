set(BOARD "virt" CACHE STRING "Board to target")
set_property(CACHE BOARD PROPERTY STRINGS virt msm)

message(STATUS "Targeting board '${BOARD}'")

if(BOARD STREQUAL "virt")
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

set_source_files_properties(
	${CMAKE_CURRENT_LIST_DIR}/loader/start.cpp
	${CMAKE_CURRENT_LIST_DIR}/loader/early_paging.cpp
	${CMAKE_CURRENT_LIST_DIR}/dtb.cpp
	TARGET_DIRECTORY crescent
	PROPERTIES COMPILE_FLAGS -mno-unaligned-access
)

add_subdirectory(dev)
add_subdirectory(interrupts)
