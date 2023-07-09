target_sources(crescent PRIVATE
        spinlock.c
        ubsan.c
        handle.c)

if(MISC_TEST)
    target_sources(crescent PRIVATE test.c)
endif()