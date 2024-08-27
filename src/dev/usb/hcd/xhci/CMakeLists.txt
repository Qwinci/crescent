if(CONFIG_USB_XHCI)
	target_sources(crescent PRIVATE
		xhci.cpp
		transfer_ring.cpp
	)

	if(CONFIG_XHCI_PCI)
		target_sources(crescent PRIVATE xhci_pci.cpp)
	endif()
endif()
