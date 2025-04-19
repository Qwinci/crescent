target_sources(crescent PRIVATE
	syscalls.cpp
	event_queue.cpp
	service.cpp
)

add_subdirectory(posix)
