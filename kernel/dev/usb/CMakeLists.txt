if(DEVICE_USB)
	add_sources(
		core/usb-bus.cpp
		core/usb-config.cpp
		core/usb-device.cpp
		core/usb-transfer.cpp
		device/usb-generic.cpp
		device/usb-keyboard.cpp
		device/usb-storage.cpp
		device/usb-hub.cpp
	)
endif()

if(DEVICE_UHCI)
	add_sources(
		hcd/uhci-hcd.cpp
		hcd/uhci-roothub.cpp
	)
endif()

if(DEVICE_OHCI)
	add_sources(
		hcd/ohci-hcd.cpp
		hcd/ohci-roothub.cpp
	)
endif()
