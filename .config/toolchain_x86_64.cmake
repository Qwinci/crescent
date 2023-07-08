include(CheckLinkerFlag)
include(CheckCXXCompilerFlag)

set(CMAKE_SYSTEM_NAME Generic)

set(triple x86_64-unknown-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET ${triple})

target_compile_options(crescent PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>: -mgeneral-regs-only -mno-red-zone>)

CHECK_CXX_COMPILER_FLAG(-mcmodel=kernel COMPILER_SUPPORTS_PIC_MCMODEL_KERNEL)
if(COMPILER_SUPPORTS_PIC_MCMODEL_KERNEL)
	target_compile_options(crescent PRIVATE $<$<COMPILE_LANGUAGE:CXX>: -mcmodel=kernel>)
endif()

target_link_options(crescent PRIVATE -static -static-pie -nostdlib -T ${PROJECT_SOURCE_DIR}/.config/kernel_x86_64.ld)
target_compile_definitions(crescent PRIVATE PAGE_SIZE=0x1000)

CHECK_CXX_COMPILER_FLAG(-nostdlibinc COMPILER_SUPPORTS_NOSTDLIBINC)
if(COMPILER_SUPPORTS_NOSTDLIBINC)
	target_compile_options(crescent PRIVATE -nostdlibinc)
endif()
