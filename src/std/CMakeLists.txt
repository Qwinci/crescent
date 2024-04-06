target_sources(crescent PRIVATE
	cstring.cpp
	stdio.cpp
	new.cpp
	cxx.cpp
)

if(NOT CMAKE_CROSSCOMPILING)
	include(FetchContent)
	FetchContent_Declare(
		googletest
		URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
	)
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)

	enable_testing()

	add_executable(std_tests
		tests.cpp
		../mem/vmem.cpp
	)
	target_compile_options(std_tests PRIVATE -Wall -Wextra -fsanitize=undefined,address)
	target_compile_definitions(std_tests PRIVATE TESTING)
	target_include_directories(std_tests PRIVATE . .. ../mem)
	target_link_libraries(std_tests GTest::gtest_main)
	target_link_options(std_tests PRIVATE -fsanitize=undefined,address)

	include(GoogleTest)
	gtest_discover_tests(std_tests)
endif()
