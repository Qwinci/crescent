target_compile_options(crescent PRIVATE
	-mno-red-zone
)
target_link_options(crescent PRIVATE -T ${PROJECT_SOURCE_DIR}/.config/kernel_x86_64.ld)
