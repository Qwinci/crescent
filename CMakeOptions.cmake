option(WITH_NVME "Build the NVMe driver" ON)
set(WITH_SCHED "Multilevel feedback queue" CACHE STRING "Scheduling algorithm to use")
set_property(CACHE WITH_SCHED PROPERTY STRINGS "Multilevel feedback queue")
option(WITH_INTEL_GRAPHICS "Build the Intel graphics driver" ON)
option(WITH_RTL8169 "Build Realtek Rtl8169 ethernet driver" ON)

if(WITH_NVME)
    add_compile_definitions(WITH_NVME)
endif()

if(WITH_SCHED STREQUAL "Multilevel feedback queue")
    add_compile_definitions(WITH_SCHED_MULTI)
else()
    message(FATAL_ERROR "Unknown scheduling algorithm")
endif()

if(WITH_INTEL_GRAPHICS)
    add_compile_definitions(WITH_INTEL_GRAPHICS)
endif()

if(WITH_RTL8169)
    add_compile_definitions(WITH_RTL8169)
endif()