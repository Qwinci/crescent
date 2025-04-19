if(CONFIG_UFS)
	target_sources(crescent PRIVATE
		ufs.cpp
	)

	if(CONFIG_UFS_PCI)
		target_sources(crescent PRIVATE ufs_pci.cpp)
	endif()
endif()
