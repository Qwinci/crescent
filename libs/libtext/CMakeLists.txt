LIB(text src/main.cpp)

target_include_directories(text PUBLIC include)
target_link_libraries(text PRIVATE common)
