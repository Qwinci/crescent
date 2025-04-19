include(uHDA/uhda.cmake)

target_sources(crescent PRIVATE
	hda.cpp
	#tmp.cpp
	#${UHDA_SOURCES}
)
target_include_directories(crescent PRIVATE ${UHDA_INCLUDES})
