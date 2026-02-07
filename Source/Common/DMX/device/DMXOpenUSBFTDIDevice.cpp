/*
  ==============================================================================

    DMXOpenUSBFTDIDevice.cpp
    Created: 7 Feb 2026
    Author:  j-mutter

    Based on QLC+ libftdi-interface.cpp

  ==============================================================================
*/

#if JUCE_MAC || JUCE_LINUX

#include <unistd.h>
#include <libusb.h>

DMXOpenUSBFTDIDevice::DMXOpenUSBFTDIDevice() :
	DMXDevice("OpenDMX FTDI", OPENDMX_FTDI, false),
	m_isOpen(false),
	m_defaultLatency(16)
{
	bzero(&m_ftdi, sizeof(struct ftdi_context));
	ftdi_init(&m_ftdi);

	deviceSelector = addEnumParameter("FTDI Device", "Select the FTDI USB device to use");
	refreshDeviceList();
}

DMXOpenUSBFTDIDevice::~DMXOpenUSBFTDIDevice()
{
	closeDevice();
	ftdi_deinit(&m_ftdi);
}

void DMXOpenUSBFTDIDevice::refreshDeviceList()
{
	String previousSelection = m_selectedSerial;
	deviceSelector->clearOptions();
	deviceSelector->addOption("None", "none");

	Array<FTDIDeviceInfo> devices = enumerateDevices();

	for (const auto& dev : devices)
	{
		String label = dev.name.isEmpty() ? dev.serial : dev.name + " (" + dev.serial + ")";
		deviceSelector->addOption(label, dev.serial);
	}

	if (previousSelection.isNotEmpty())
	{
		deviceSelector->setValueWithData(previousSelection);
	}
}

Array<FTDIDeviceInfo> DMXOpenUSBFTDIDevice::enumerateDevices()
{
	Array<FTDIDeviceInfo> result;

	struct ftdi_context ftdi;
	ftdi_init(&ftdi);

	libusb_device** devs;
	if (libusb_get_device_list(ftdi.usb_ctx, &devs) < 0)
	{
		DBG("FTDI: libusb_get_device_list() failed");
		ftdi_deinit(&ftdi);
		return result;
	}

	libusb_device* dev;
	int i = 0;
	while ((dev = devs[i++]) != nullptr)
	{
		struct libusb_device_descriptor desc;
		libusb_get_device_descriptor(dev, &desc);

		if (desc.idVendor != FTDI_VID)
			continue;

		if (desc.idProduct != FTDI_PID && desc.idProduct != FTDI2_PID)
			continue;

		char ser[256];
		char nme[256];
		char vend[256];
		memset(ser, 0, 256);
		memset(nme, 0, 256);
		memset(vend, 0, 256);

		ftdi_usb_get_strings(&ftdi, dev, vend, 256, nme, 256, ser, 256);

		FTDIDeviceInfo info;
		info.serial = String(ser);
		info.name = String(nme);
		info.vendor = String(vend);
		info.vid = desc.idVendor;
		info.pid = desc.idProduct;

		DBG("FTDI: Found device VID:" + String::toHexString(info.vid)
			+ " PID:" + String::toHexString(info.pid)
			+ " serial:" + info.serial
			+ " name:" + info.name);

		result.add(info);
	}

	libusb_free_device_list(devs, 1);
	ftdi_deinit(&ftdi);

	return result;
}

bool DMXOpenUSBFTDIDevice::openDevice(const String& serial, const String& name)
{
	if (m_isOpen)
		closeDevice();

	const char* ser = serial.isNotEmpty() ? serial.toRawUTF8() : nullptr;
	const char* nme = name.isNotEmpty() ? name.toRawUTF8() : nullptr;

	if (ftdi_usb_open_desc(&m_ftdi, FTDI_VID, FTDI_PID, nme, ser) < 0)
	{
		// Try alternate PID
		if (ftdi_usb_open_desc(&m_ftdi, FTDI_VID, FTDI2_PID, nme, ser) < 0)
		{
			LOGWARNING("FTDI: Could not open device: " + String(ftdi_get_error_string(&m_ftdi)));
			return false;
		}
	}

	if (ftdi_get_latency_timer(&m_ftdi, &m_defaultLatency) < 0)
	{
		DBG("FTDI: Could not query default latency, using 16ms");
		m_defaultLatency = 16;
	}

	if (ftdi_usb_reset(&m_ftdi) < 0)
	{
		LOGWARNING("FTDI: Reset failed: " + String(ftdi_get_error_string(&m_ftdi)));
		ftdi_usb_close(&m_ftdi);
		return false;
	}

	if (ftdi_set_baudrate(&m_ftdi, 250000) < 0)
	{
		LOGWARNING("FTDI: Set baud rate failed: " + String(ftdi_get_error_string(&m_ftdi)));
		ftdi_usb_close(&m_ftdi);
		return false;
	}

	if (ftdi_set_line_property(&m_ftdi, BITS_8, STOP_BIT_2, NONE) < 0)
	{
		LOGWARNING("FTDI: Set line property failed: " + String(ftdi_get_error_string(&m_ftdi)));
		ftdi_usb_close(&m_ftdi);
		return false;
	}

	if (ftdi_setflowctrl(&m_ftdi, SIO_DISABLE_FLOW_CTRL) < 0)
	{
		LOGWARNING("FTDI: Set flow control failed: " + String(ftdi_get_error_string(&m_ftdi)));
		ftdi_usb_close(&m_ftdi);
		return false;
	}

	if (ftdi_set_latency_timer(&m_ftdi, 1) < 0)
	{
		DBG("FTDI: Could not set low latency, using default");
	}

	if (ftdi_setrts(&m_ftdi, 0) < 0)
	{
		DBG("FTDI: Could not clear RTS");
	}

#if defined(LIBFTDI1_5)
	if (ftdi_tcioflush(&m_ftdi) < 0)
#else
	if (ftdi_usb_purge_buffers(&m_ftdi) < 0)
#endif
	{
		DBG("FTDI: Could not purge buffers");
	}

	m_isOpen = true;
	m_selectedSerial = serial;
	NLOG(niceName, "FTDI device opened: " + name + " (" + serial + ")");
	setConnected(true);
	return true;
}

void DMXOpenUSBFTDIDevice::closeDevice()
{
	if (!m_isOpen)
		return;

	// Restore default latency before closing
	ftdi_set_latency_timer(&m_ftdi, m_defaultLatency);
	ftdi_usb_close(&m_ftdi);
	m_isOpen = false;
	setConnected(false);
	NLOG(niceName, "FTDI device closed");
}

void DMXOpenUSBFTDIDevice::sendDMXValuesInternal()
{
	if (!m_isOpen)
		return;

	// DMX Break signal (~110us)
	if (ftdi_set_line_property2(&m_ftdi, BITS_8, STOP_BIT_2, NONE, BREAK_ON) < 0)
	{
		DBG("FTDI: Break ON failed");
		return;
	}

	usleep(110);

	// Mark-After-Break (~16us)
	if (ftdi_set_line_property2(&m_ftdi, BITS_8, STOP_BIT_2, NONE, BREAK_OFF) < 0)
	{
		DBG("FTDI: Break OFF failed");
		return;
	}

	usleep(16);

	// Send start code (0x00) + 512 DMX channels
	uint8 dmxFrame[513];
	dmxFrame[0] = 0x00; // DMX start code
	memcpy(dmxFrame + 1, dmxDataOut, 512);

	if (ftdi_write_data(&m_ftdi, dmxFrame, 513) < 0)
	{
		DBG("FTDI: Write failed: " + String(ftdi_get_error_string(&m_ftdi)));
	}
}

void DMXOpenUSBFTDIDevice::onContainerParameterChanged(Parameter* p)
{
	DMXDevice::onContainerParameterChanged(p);

	if (p == deviceSelector)
	{
		String selectedSerial = deviceSelector->getValueData().toString();

		if (selectedSerial == "none" || selectedSerial.isEmpty())
		{
			closeDevice();
		}
		else
		{
			// Find the device info for the selected serial to get the name
			Array<FTDIDeviceInfo> devices = enumerateDevices();
			String deviceName;
			for (const auto& dev : devices)
			{
				if (dev.serial == selectedSerial)
				{
					deviceName = dev.name;
					break;
				}
			}
			openDevice(selectedSerial, deviceName);
		}
	}
}

#else

// Stub implementation for unsupported platforms
DMXOpenUSBFTDIDevice::DMXOpenUSBFTDIDevice() :
	DMXDevice("OpenDMX FTDI", OPENDMX_FTDI, false)
{
	LOGWARNING("FTDI DMX is not supported on this platform");
}

DMXOpenUSBFTDIDevice::~DMXOpenUSBFTDIDevice()
{
}

void DMXOpenUSBFTDIDevice::sendDMXValuesInternal()
{
}

#endif
