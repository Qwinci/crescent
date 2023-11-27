include(ExternalProject)

find_program(MESON meson REQUIRED)
find_program(NINJA ninja REQUIRED)

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/libs/mlibc)
	find_program(GIT git REQUIRED)
	execute_process(
		COMMAND ${GIT} clone https://github.com/qwinci/mlibc --depth=1 ${CMAKE_SOURCE_DIR}/libs/mlibc
	)
endif()

set(mlibc_config "--cross-file=${CMAKE_SOURCE_DIR}/libs/cross-file.toml" --buildtype=debug)
ExternalProject_Add(mlibc
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/libs/mlibc
	PREFIX mlibc-prefix
	INSTALL_DIR ${CMAKE_BINARY_DIR}/sysroot
	CONFIGURE_COMMAND
		env CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CXXFLAGS="-Wno-ignored-attributes"
		${MESON} setup ${mlibc_config} --prefix=<INSTALL_DIR>/usr <BINARY_DIR> <SOURCE_DIR>
	BUILD_COMMAND
		${NINJA} -C <BINARY_DIR>
	INSTALL_COMMAND
		${NINJA} -C <BINARY_DIR> install
		COMMAND touch <INSTALL_DIR>/usr/lib/libunwind.a
)
