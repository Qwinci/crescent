target_sources(crescent PRIVATE
        fb.c
        con.c
        fbcon.c
        file.c
        pci_enum.c)

if(ARCH STREQUAL "x86_64")
    add_subdirectory(x86)
endif()