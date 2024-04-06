target_include_directories(crescent PRIVATE include)
target_sources(crescent PRIVATE
	generic.cpp
	../aarch64/kernel_dtb.cpp
	../aarch64/dtb.cpp
)
