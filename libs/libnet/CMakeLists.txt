LIB(net src/dns.cpp)

target_include_directories(net PUBLIC include)
target_link_libraries(net PRIVATE common)
target_link_libraries(net PUBLIC libcxx)
