if(DEVICE_ATA)
	add_sources(
		ata-pci.cpp # XXX should this be configurable?
		ata-controller.cpp
		atadisk.cpp
		atacd.cpp
	)
endif()
