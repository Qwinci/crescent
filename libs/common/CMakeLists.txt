LIB(common src/start.cpp)
target_sources(common PRIVATE
	src/sys.cpp
)

target_include_directories(common PUBLIC include)
