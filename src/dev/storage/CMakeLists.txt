target_sources(crescent PRIVATE
        storage.c
        nvme.c

        gpt.c
        )

add_subdirectory(fs)