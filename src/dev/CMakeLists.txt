target_sources(crescent PRIVATE
        fb.c
        con.c
        fbcon.c
        file.c
        pci_enum.c
        pci.c
        timer.c)

if(ARCH STREQUAL "x86_64")
    add_subdirectory(x86)
endif()

add_subdirectory(storage)
add_subdirectory(audio)
add_subdirectory(usb)
