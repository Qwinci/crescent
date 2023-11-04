target_sources(crescent PRIVATE
        spinlock.c
        ubsan.c
        handle.c
        rb_tree.c
        cmdline.c
)

if(MISC_TEST)
    target_sources(crescent PRIVATE test.c)
endif()