target_sources(crescent PRIVATE
        string.c
        stdio.c
        inttypes.c
        ctype.c)
target_include_directories(crescent PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")