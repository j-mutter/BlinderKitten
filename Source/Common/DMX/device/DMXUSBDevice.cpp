/*
  ==============================================================================

    DMXUSBDevice.cpp
    Created: 16 Feb 2026
    Author:  j-mutter

    Unified USB DMX device implementation.

  ==============================================================================
*/

// Enttec Pro framing constants (reuse from DMXEnttecProDevice.h)
#define USB_DMX_START_MSG  0x7E
#define USB_DMX_END_MSG    0xE7
#define USB_DMX_SEND_LABEL 0x06
#define USB_DMX_RECV_LABEL 5
#define USB_DMX_RECV_ON_CHANGE_LABEL 8
#define USB_DMX_SERIAL_NUMBER_LABEL 10

#define DMXKING_SEND_PORT1_LABEL 0x64

static const uint16 FTDI_VID = 0x0403;

#if JUCE_MAC || JUCE_LINUX

#include <unistd.h>
#include <libusb.h>
#if JUCE_LINUX
#include <asm/termbits.h>
#elif JUCE_MAC
#include <IOKit/serial/ioss.h>
#endif

// ============================================================================
// Construction / Destruction
// ============================================================================

DMXUSBDevice::DMXUSBDevice() :
	DMXDevice("USB DMX", USB_DMX, true),
	activeMode(USBDMX_OPEN_DMX),
	ftdiIsOpen(false),
	ftdiDefaultLatency(16),
	serialPort(nullptr)
{
	inputCC->enabled->setValue(false);

	bzero(&ftdi, sizeof(struct ftdi_context));
	ftdi_init(&ftdi);

	deviceSelector = addEnumParameter("Device", "Select the USB DMX device to use");
	deviceSelector->addOption("None", "none");

	modeSelector = addEnumParameter("Mode", "Protocol mode. Auto detects from device name.");
	modeSelector->addOption("Auto", USBDMX_AUTO);
	modeSelector->addOption("Open DMX", USBDMX_OPEN_DMX);
	modeSelector->addOption("Enttec Pro", USBDMX_ENTTEC_PRO);
	modeSelector->addOption("DMXKing", USBDMX_DMXKING);
	modeSelector->addOption("Eurolite", USBDMX_EUROLITE);
	modeSelector->setValueWithKey("Auto");

	outputPortSelector = addEnumParameter("Output Port", "DMXKing output port");
	outputPortSelector->addOption("Port A", 1);
	outputPortSelector->hideInEditor = true;
	dmxKingPortCount = 1;

	SerialManager::getInstance()->addSerialManagerListener(this);
	refreshDeviceList();
	startTimer(2000);
}

DMXUSBDevice::~DMXUSBDevice()
{
	stopTimer();
	closeConnection();

	if (SerialManager::getInstanceWithoutCreating() != nullptr)
		SerialManager::getInstance()->removeSerialManagerListener(this);

	ftdi_deinit(&ftdi);
}

// ============================================================================
// Auto-Detection Heuristics
// ============================================================================

USBDMXMode DMXUSBDevice::autoDetectMode(const String& name, uint16 vid, uint16 pid)
{
	String upper = name.toUpperCase();

	if (upper.contains("PRO MK2") || upper.contains("PRO MK 2"))
		return USBDMX_ENTTEC_PRO;

	if (upper.contains("DMX USB PRO") || upper.contains("ENTTEC"))
		return USBDMX_ENTTEC_PRO;

	if (upper.contains("DMXKING") || upper.contains("DMX KING") || upper.contains("ULTRA"))
		return USBDMX_DMXKING;

	if (vid == 0x04D8 && pid == 0xFA63)
		return USBDMX_EUROLITE;

	if (upper.contains("EUROLITE"))
		return USBDMX_EUROLITE;

	return USBDMX_OPEN_DMX;
}

String DMXUSBDevice::modeToString(USBDMXMode mode)
{
	switch (mode)
	{
	case USBDMX_OPEN_DMX:   return "Open DMX";
	case USBDMX_ENTTEC_PRO: return "Enttec Pro";
	case USBDMX_DMXKING:    return "DMXKing";
	case USBDMX_EUROLITE:   return "Eurolite";
	default:                 return "Auto";
	}
}

// ============================================================================
// Device Enumeration
// ============================================================================

void DMXUSBDevice::refreshDeviceList()
{
	String previousID = deviceSelector->getValueData().toString();

	detectedDevices.clear();
	deviceSelector->clearOptions();
	deviceSelector->addOption("None", "none");

	// Collect FTDI serial numbers for dedup
	StringArray ftdiSerials;

	// 1. Enumerate FTDI devices
	Array<FTDIDeviceInfo> ftdiDevs = DMXOpenUSBFTDIDevice::enumerateDevices();
	for (const auto& fd : ftdiDevs)
	{
		DetectedUSBDevice d;
		d.name = fd.name;
		d.serial = fd.serial;
		d.portPath = "";
		d.ftdiIndex = -1;
		d.vid = fd.vid;
		d.pid = fd.pid;
		d.isFTDI = true;
		d.detectedMode = autoDetectMode(fd.name, fd.vid, fd.pid);
		detectedDevices.add(d);
		ftdiSerials.add(fd.serial);
	}

	// Update hot-swap tracking
	lastFTDISerials = ftdiSerials;

	// 2. Enumerate serial devices (filter: non-zero VID/PID, skip FTDI VID duplicates)
	for (auto* info : SerialManager::getInstance()->portInfos)
	{
		if (info->vid == 0 && info->pid == 0)
			continue; // skip virtual/bluetooth ports

		// Skip if this is an FTDI device already found via FTDI enumeration
		if (info->vid == FTDI_VID)
			continue;

		DetectedUSBDevice d;
		d.name = info->description;
		d.serial = info->hardwareID;
		d.portPath = info->port;
		d.ftdiIndex = -1;
		d.vid = (uint16)info->vid;
		d.pid = (uint16)info->pid;
		d.isFTDI = false;
		d.detectedMode = autoDetectMode(info->description, d.vid, d.pid);
		detectedDevices.add(d);
	}

	// 3. Populate dropdown
	for (const auto& d : detectedDevices)
	{
		String label = d.getDisplayLabel() + " [" + modeToString(d.detectedMode) + (d.isFTDI ? ", FTDI" : ", Serial") + "]";
		deviceSelector->addOption(label, d.getUniqueID());
	}

	// Restore previous selection
	if (previousID.isNotEmpty() && previousID != "none")
	{
		deviceSelector->setValueWithData(previousID);
	}
}

// ============================================================================
// Parameter Change Handler
// ============================================================================

void DMXUSBDevice::onContainerParameterChanged(Parameter* p)
{
	DMXDevice::onContainerParameterChanged(p);

	if (p == deviceSelector)
	{
		String selectedID = deviceSelector->getValueData().toString();

		// Skip if already connected to this device
		if (selectedID == currentDevice.getUniqueID() &&
			(ftdiIsOpen || serialPort != nullptr))
			return;

		closeConnection();

		if (selectedID == "none" || selectedID.isEmpty())
			return;

		// Find the detected device
		for (const auto& d : detectedDevices)
		{
			if (d.getUniqueID() == selectedID)
			{
				currentDevice = d;
				break;
			}
		}

		// Determine active mode
		USBDMXMode selectedMode = (USBDMXMode)(int)modeSelector->getValueData();
		if (selectedMode == USBDMX_AUTO)
			activeMode = currentDevice.detectedMode;
		else
			activeMode = selectedMode;

		openConnection();
	}
	else if (p == modeSelector)
	{
		String selectedID = deviceSelector->getValueData().toString();
		if (selectedID == "none" || selectedID.isEmpty())
			return;

		USBDMXMode selectedMode = (USBDMXMode)(int)modeSelector->getValueData();
		USBDMXMode newMode = (selectedMode == USBDMX_AUTO) ? currentDevice.detectedMode : selectedMode;

		if (newMode != activeMode)
		{
			closeConnection();
			activeMode = newMode;
			openConnection();
		}
	}
}

// ============================================================================
// Timer - FTDI Hot-Swap
// ============================================================================

void DMXUSBDevice::timerCallback()
{
	// Re-scan FTDI devices to detect hot-plug/unplug
	Array<FTDIDeviceInfo> ftdiDevs = DMXOpenUSBFTDIDevice::enumerateDevices();

	StringArray currentSerials;
	for (const auto& fd : ftdiDevs)
		currentSerials.add(fd.serial);

	if (currentSerials == lastFTDISerials)
		return;

	// FTDI device list changed
	lastFTDISerials = currentSerials;

	// If our FTDI device was unplugged, close the connection
	if (ftdiIsOpen && currentDevice.isFTDI && !currentSerials.contains(currentDevice.serial))
	{
		NLOG(niceName, "FTDI device disconnected: " + currentDevice.serial);
		closeConnection();
	}

	refreshDeviceList();

	// If we have a selection but aren't connected, try to reconnect
	String selectedID = deviceSelector->getValueData().toString();
	if (selectedID != "none" && !selectedID.isEmpty() &&
		!ftdiIsOpen && serialPort == nullptr)
	{
		for (const auto& d : detectedDevices)
		{
			if (d.getUniqueID() == selectedID)
			{
				currentDevice = d;
				USBDMXMode selectedMode = (USBDMXMode)(int)modeSelector->getValueData();
				activeMode = (selectedMode == USBDMX_AUTO) ? currentDevice.detectedMode : selectedMode;
				openConnection();
				break;
			}
		}
	}
}

// ============================================================================
// Connection Management
// ============================================================================

void DMXUSBDevice::openConnection()
{
	if (currentDevice.isFTDI)
	{
		if (!openFTDI(currentDevice.serial, currentDevice.name))
			return;

		// If auto-detect guessed Enttec Pro, probe to see if it's actually a DMXKing
		USBDMXMode selectedMode = (USBDMXMode)(int)modeSelector->getValueData();
		if (selectedMode == USBDMX_AUTO && activeMode == USBDMX_ENTTEC_PRO && probeDMXKingFTDI())
		{
			NLOG(niceName, "Device identified as DMXKing via firmware probe");
			activeMode = USBDMX_DMXKING;
			currentDevice.detectedMode = USBDMX_DMXKING;
			updateDeviceLabel(currentDevice.getUniqueID(), USBDMX_DMXKING);
		}

		// If DMXKing (auto-detected or manually selected), probe port count
		if (activeMode == USBDMX_DMXKING)
			probeDMXKingFTDI();

		updateOutputPortVisibility();
	}
	else if (currentDevice.portPath.isNotEmpty())
	{
		openSerial(currentDevice.portPath);
		updateOutputPortVisibility();
	}
}

void DMXUSBDevice::closeConnection()
{
	closeFTDI();
	closeSerial();
	serialBuffer.clear();
	updateOutputPortVisibility();
}

// ============================================================================
// FTDI Operations (Mac/Linux - libftdi1)
// ============================================================================

bool DMXUSBDevice::openFTDI(const String& serial, const String& name)
{
	if (ftdiIsOpen)
		closeFTDI();

	const char* ser = serial.isNotEmpty() ? serial.toRawUTF8() : nullptr;
	const char* nme = name.isNotEmpty() ? name.toRawUTF8() : nullptr;

	if (ftdi_usb_open_desc(&ftdi, FTDI_VID, 0x6001, nme, ser) < 0)
	{
		if (ftdi_usb_open_desc(&ftdi, FTDI_VID, 0x6010, nme, ser) < 0)
		{
			LOGWARNING("USB DMX: Could not open FTDI device: " + String(ftdi_get_error_string(&ftdi)));
			return false;
		}
	}

	if (ftdi_get_latency_timer(&ftdi, &ftdiDefaultLatency) < 0)
		ftdiDefaultLatency = 16;

	if (ftdi_usb_reset(&ftdi) < 0)
	{
		LOGWARNING("USB DMX: FTDI reset failed");
		ftdi_usb_close(&ftdi);
		return false;
	}

	// Configure based on protocol mode
	int baudRate = 250000;
	enum ftdi_stopbits_type stopBits = STOP_BIT_2;

	if (activeMode == USBDMX_ENTTEC_PRO || activeMode == USBDMX_DMXKING)
	{
		baudRate = 115200;
		stopBits = STOP_BIT_1;
	}

	if (ftdi_set_baudrate(&ftdi, baudRate) < 0)
	{
		LOGWARNING("USB DMX: FTDI set baud rate failed");
		ftdi_usb_close(&ftdi);
		return false;
	}

	if (ftdi_set_line_property(&ftdi, BITS_8, stopBits, NONE) < 0)
	{
		LOGWARNING("USB DMX: FTDI set line property failed");
		ftdi_usb_close(&ftdi);
		return false;
	}

	if (ftdi_setflowctrl(&ftdi, SIO_DISABLE_FLOW_CTRL) < 0)
	{
		LOGWARNING("USB DMX: FTDI set flow control failed");
		ftdi_usb_close(&ftdi);
		return false;
	}

	ftdi_set_latency_timer(&ftdi, 1);
	ftdi_setrts(&ftdi, 0);

#if defined(LIBFTDI1_5)
	ftdi_tcioflush(&ftdi);
#else
	ftdi_usb_purge_buffers(&ftdi);
#endif

	ftdiIsOpen = true;
	NLOG(niceName, "FTDI device opened: " + name + " (" + serial + ") mode: " + modeToString(activeMode));
	setConnected(true);
	return true;
}

void DMXUSBDevice::closeFTDI()
{
	if (!ftdiIsOpen)
		return;

	ftdi_set_latency_timer(&ftdi, ftdiDefaultLatency);
	ftdi_usb_close(&ftdi);
	ftdiIsOpen = false;
	setConnected(false);
}

void DMXUSBDevice::sendFTDI_OpenDMX()
{
	if (!ftdiIsOpen) return;

	if (ftdi_set_line_property2(&ftdi, BITS_8, STOP_BIT_2, NONE, BREAK_ON) < 0) return;
	usleep(110);
	if (ftdi_set_line_property2(&ftdi, BITS_8, STOP_BIT_2, NONE, BREAK_OFF) < 0) return;
	usleep(16);

	uint8 dmxFrame[513];
	dmxFrame[0] = 0x00;
	memcpy(dmxFrame + 1, dmxDataOut, 512);
	ftdi_write_data(&ftdi, dmxFrame, 513);
}

void DMXUSBDevice::sendFTDI_EnttecPro()
{
	if (!ftdiIsOpen) return;

	uint16 len = 513;
	uint8 header[5] = { USB_DMX_START_MSG, USB_DMX_SEND_LABEL, (uint8)(len & 0xFF), (uint8)(len >> 8), 0x00 };
	uint8 footer[1] = { USB_DMX_END_MSG };

	ftdi_write_data(&ftdi, header, 5);
	ftdi_write_data(&ftdi, dmxDataOut, 512);
	ftdi_write_data(&ftdi, footer, 1);
}

void DMXUSBDevice::sendFTDI_DMXKing()
{
	if (!ftdiIsOpen) return;

	int port = (int)outputPortSelector->getValueData();
	uint8 sendLabel = DMXKING_SEND_PORT1_LABEL + (port - 1);

	uint16 len = 513;
	uint8 header[5] = { USB_DMX_START_MSG, sendLabel, (uint8)(len & 0xFF), (uint8)(len >> 8), 0x00 };
	uint8 footer[1] = { USB_DMX_END_MSG };

	ftdi_write_data(&ftdi, header, 5);
	ftdi_write_data(&ftdi, dmxDataOut, 512);
	ftdi_write_data(&ftdi, footer, 1);
}

void DMXUSBDevice::sendFTDI_Eurolite()
{
	if (!ftdiIsOpen) return;

	// Eurolite needs break signal even with framed protocol
	if (ftdi_set_line_property2(&ftdi, BITS_8, STOP_BIT_2, NONE, BREAK_ON) < 0) return;
	usleep(110);
	if (ftdi_set_line_property2(&ftdi, BITS_8, STOP_BIT_2, NONE, BREAK_OFF) < 0) return;
	usleep(16);

	uint16 len = 513;
	uint8 header[5] = { USB_DMX_START_MSG, USB_DMX_SEND_LABEL, (uint8)(len & 0xFF), (uint8)(len >> 8), 0x00 };
	uint8 footer[1] = { USB_DMX_END_MSG };

	ftdi_write_data(&ftdi, header, 5);
	ftdi_write_data(&ftdi, dmxDataOut, 512);
	ftdi_write_data(&ftdi, footer, 1);
}

// ============================================================================
// Serial Operations (Mac/Linux)
// ============================================================================

bool DMXUSBDevice::openSerial(const String& portPath)
{
	closeSerial();

	SerialDeviceInfo* info = nullptr;
	for (auto* pi : SerialManager::getInstance()->portInfos)
	{
		if (pi->port == portPath)
		{
			info = pi;
			break;
		}
	}

	if (info == nullptr) return false;

	serialPort = SerialManager::getInstance()->getPort(info);
	if (serialPort == nullptr) return false;

	serialPort->addSerialDeviceListener(this);
	serialPort->setMode(SerialDevice::PortMode::RAW);
	serialPort->open();

	if (!serialPort->isOpen())
	{
		NLOG(niceName, "Could not open serial port: " + portPath);
		serialPort->removeSerialDeviceListener(this);
		serialPort = nullptr;
		return false;
	}

	lastOpenedPortID = portPath;

	// Configure based on mode
	switch (activeMode)
	{
	case USBDMX_OPEN_DMX:   configureSerialPort_OpenDMX(); break;
	case USBDMX_ENTTEC_PRO: configureSerialPort_EnttecPro(); break;
	case USBDMX_DMXKING:    configureSerialPort_EnttecPro(); break;
	case USBDMX_EUROLITE:   configureSerialPort_Eurolite(); break;
	default: break;
	}

	NLOG(niceName, "Serial port opened: " + portPath + " mode: " + modeToString(activeMode));
	setConnected(true);
	return true;
}

void DMXUSBDevice::closeSerial()
{
	if (serialPort != nullptr)
	{
		serialPort->removeSerialDeviceListener(this);
		serialPort = nullptr;
		setConnected(false);
	}
}

void DMXUSBDevice::configureSerialPort_OpenDMX()
{
	if (serialPort == nullptr || serialPort->port == nullptr) return;
	try
	{
		serialPort->port->setBaudrate(250000);
		serialPort->port->setBytesize(serial::eightbits);
		serialPort->port->setStopbits(serial::stopbits_two);
		serialPort->port->setParity(serial::parity_none);
		serialPort->port->setFlowcontrol(serial::flowcontrol_none);
		serialPort->port->setRTS(false);
		serialPort->port->setDTR(false);
		serialPort->port->flush();

#if JUCE_LINUX
#if defined(TCGETS2)
		int fd = serialPort->port->getHandle();
		struct termios2 tio;
		if (ioctl(fd, TCGETS2, &tio) >= 0)
		{
			tio.c_cflag &= ~CBAUD;
			tio.c_cflag |= BOTHER;
			tio.c_ispeed = 250000;
			tio.c_ospeed = 250000;
			ioctl(fd, TCSETS2, &tio);
		}
#endif
#elif JUCE_MAC
		int fd = serialPort->port->getHandle();
		speed_t new_baud = static_cast<speed_t>(250000);
		ioctl(fd, IOSSIOSPEED, &new_baud, 1);
#endif
	}
	catch (serial::IOException e)
	{
		LOGERROR("USB DMX: Error configuring Open DMX port: " << e.what());
	}
}

void DMXUSBDevice::configureSerialPort_EnttecPro()
{
	if (serialPort == nullptr || serialPort->port == nullptr) return;
	try
	{
		serialPort->port->setBaudrate(115200);
		serialPort->port->setBytesize(serial::eightbits);
		serialPort->port->setStopbits(serial::stopbits_one);
		serialPort->port->setParity(serial::parity_none);
		serialPort->port->flush();

		// Request serial number
		uint8 getSerialCmd[5] = { USB_DMX_START_MSG, (uint8)USB_DMX_SERIAL_NUMBER_LABEL, 0, 0, USB_DMX_END_MSG };
		serialPort->port->write(getSerialCmd, 5);

		// Enable receive-on-change
		uint8 changeAlways[6] = { USB_DMX_START_MSG, (uint8)USB_DMX_RECV_ON_CHANGE_LABEL, 1, 0, 0, USB_DMX_END_MSG };
		serialPort->port->write(changeAlways, 6);
	}
	catch (serial::IOException e)
	{
		LOGERROR("USB DMX: Error configuring Enttec Pro port: " << e.what());
	}
}

void DMXUSBDevice::configureSerialPort_Eurolite()
{
	if (serialPort == nullptr || serialPort->port == nullptr) return;
	try
	{
		serialPort->port->setBaudrate(250000);
		serialPort->port->setBytesize(serial::eightbits);
		serialPort->port->setStopbits(serial::stopbits_two);
		serialPort->port->setParity(serial::parity_none);
		serialPort->port->setFlowcontrol(serial::flowcontrol_none);
		serialPort->port->setRTS(false);
		serialPort->port->setDTR(false);
		serialPort->port->flush();
	}
	catch (serial::IOException e)
	{
		LOGERROR("USB DMX: Error configuring Eurolite port: " << e.what());
	}
}

void DMXUSBDevice::sendSerial_OpenDMX()
{
	if (serialPort == nullptr || !serialPort->isOpen()) return;
	try
	{
		serialPort->port->setBreak(true);
		serialPort->port->setBreak(false);
		uint8 startCode = 0x00;
		serialPort->port->write(&startCode, 1);
		serialPort->port->write(dmxDataOut, 512);
	}
	catch (std::exception& e)
	{
		DBG("USB DMX Serial OpenDMX send error: " << e.what());
	}
}

void DMXUSBDevice::sendSerial_EnttecPro()
{
	if (serialPort == nullptr || !serialPort->isOpen()) return;
	try
	{
		uint16 len = 513;
		uint8 header[5] = { USB_DMX_START_MSG, USB_DMX_SEND_LABEL, (uint8)(len & 0xFF), (uint8)(len >> 8), 0x00 };
		uint8 footer[1] = { USB_DMX_END_MSG };

		serialPort->port->write(header, 5);
		serialPort->port->write(dmxDataOut, 512);
		serialPort->port->write(footer, 1);
		serialPort->port->flush();

		if (inputCC->enabled->boolValue())
		{
			uint8 changeAlways[6] = { USB_DMX_START_MSG, (uint8)USB_DMX_RECV_ON_CHANGE_LABEL, 1, 0, 0, USB_DMX_END_MSG };
			serialPort->port->write(changeAlways, 6);
		}
	}
	catch (std::exception& e)
	{
		DBG("USB DMX Serial EnttecPro send error: " << e.what());
	}
}

void DMXUSBDevice::sendSerial_DMXKing()
{
	if (serialPort == nullptr || !serialPort->isOpen()) return;
	try
	{
		int port = (int)outputPortSelector->getValueData();
		uint8 sendLabel = DMXKING_SEND_PORT1_LABEL + (port - 1);

		uint16 len = 513;
		uint8 header[5] = { USB_DMX_START_MSG, sendLabel, (uint8)(len & 0xFF), (uint8)(len >> 8), 0x00 };
		uint8 footer[1] = { USB_DMX_END_MSG };

		serialPort->port->write(header, 5);
		serialPort->port->write(dmxDataOut, 512);
		serialPort->port->write(footer, 1);
		serialPort->port->flush();
	}
	catch (std::exception& e)
	{
		DBG("USB DMX Serial DMXKing send error: " << e.what());
	}
}

void DMXUSBDevice::sendSerial_Eurolite()
{
	if (serialPort == nullptr || !serialPort->isOpen()) return;
	try
	{
		serialPort->port->setBreak(true);
		serialPort->port->setBreak(false);

		uint16 len = 513;
		uint8 header[5] = { USB_DMX_START_MSG, USB_DMX_SEND_LABEL, (uint8)(len & 0xFF), (uint8)(len >> 8), 0x00 };
		uint8 footer[1] = { USB_DMX_END_MSG };

		serialPort->port->write(header, 5);
		serialPort->port->write(dmxDataOut, 512);
		serialPort->port->write(footer, 1);
		serialPort->port->flush();
	}
	catch (std::exception& e)
	{
		DBG("USB DMX Serial Eurolite send error: " << e.what());
	}
}

// ============================================================================
// DMXKing Detection & Port Visibility
// ============================================================================

bool DMXUSBDevice::probeDMXKingFTDI()
{
	if (!ftdiIsOpen) return false;

	// Send DMXKing port count query (label 0x63)
	uint8 probe[5] = { 0x7E, 0x63, 0x00, 0x00, 0xE7 };
	if (ftdi_write_data(&ftdi, probe, 5) < 5)
	{
		NLOG(niceName, "DMXKing probe: write failed");
		return false;
	}

	// Use a short read timeout so we don't block too long
	int savedTimeout = ftdi.usb_read_timeout;
	ftdi.usb_read_timeout = 100;

	// Try a few reads -- FTDI may return modem status bytes before actual data
	for (int attempt = 0; attempt < 5; attempt++)
	{
		uint8 buf[64];
		int n = ftdi_read_data(&ftdi, buf, sizeof(buf));

		for (int i = 0; i + 1 < n; i++)
		{
			if (buf[i] == 0x7E && buf[i + 1] == 0x63)
			{
				// Extract port count from response
				// Response format: 0x7E 0x63 [len_lo] [len_hi] [port_count] ... 0xE7
				if (i + 4 < n)
				{
					int portCount = (int)buf[i + 4];
					if (portCount >= 1 && portCount <= 8)
						dmxKingPortCount = portCount;
				}

				ftdi.usb_read_timeout = savedTimeout;
				NLOG(niceName, "DMXKing probe: positive response");
				return true;
			}
		}
	}

	ftdi.usb_read_timeout = savedTimeout;
	NLOG(niceName, "DMXKing probe: no response (device is likely Enttec Pro compatible)");
	return false;
}

void DMXUSBDevice::updateDeviceLabel(const String& uniqueID, USBDMXMode newMode)
{
	for (int i = 0; i < detectedDevices.size(); i++)
	{
		if (detectedDevices[i].getUniqueID() == uniqueID)
		{
			detectedDevices.getReference(i).detectedMode = newMode;
			break;
		}
	}

	// Rebuild dropdown options with updated label
	String previousID = deviceSelector->getValueData().toString();
	deviceSelector->clearOptions();
	deviceSelector->addOption("None", "none");

	for (const auto& d : detectedDevices)
	{
		String label = d.getDisplayLabel() + " [" + modeToString(d.detectedMode) + (d.isFTDI ? ", FTDI" : ", Serial") + "]";
		deviceSelector->addOption(label, d.getUniqueID());
	}

	if (previousID.isNotEmpty() && previousID != "none")
		deviceSelector->setValueWithData(previousID);
}

void DMXUSBDevice::updateOutputPortVisibility()
{
	bool showPorts = (activeMode == USBDMX_DMXKING);

	if (showPorts)
	{
		// Rebuild port options based on detected count
		String previousPort = outputPortSelector->getValueData().toString();
		outputPortSelector->clearOptions();

		const char* portNames[] = { "Port A", "Port B", "Port C", "Port D", "Port E", "Port F", "Port G", "Port H" };
		for (int i = 0; i < dmxKingPortCount && i < 8; i++)
			outputPortSelector->addOption(portNames[i], i + 1);

		if (previousPort.isNotEmpty())
			outputPortSelector->setValueWithData(previousPort);
	}

	outputPortSelector->hideInEditor = !showPorts;
	queuedNotifier.addMessage(new ContainerAsyncEvent(ContainerAsyncEvent::ControllableContainerNeedsRebuild, this));
}

// ============================================================================
// Send DMX Values
// ============================================================================

void DMXUSBDevice::sendDMXValuesInternal()
{
	if (ftdiIsOpen)
	{
		switch (activeMode)
		{
		case USBDMX_OPEN_DMX:   sendFTDI_OpenDMX(); break;
		case USBDMX_ENTTEC_PRO: sendFTDI_EnttecPro(); break;
		case USBDMX_DMXKING:    sendFTDI_DMXKing(); break;
		case USBDMX_EUROLITE:   sendFTDI_Eurolite(); break;
		default: break;
		}
	}
	else if (serialPort != nullptr && serialPort->isOpen())
	{
		switch (activeMode)
		{
		case USBDMX_OPEN_DMX:   sendSerial_OpenDMX(); break;
		case USBDMX_ENTTEC_PRO: sendSerial_EnttecPro(); break;
		case USBDMX_DMXKING:    sendSerial_DMXKing(); break;
		case USBDMX_EUROLITE:   sendSerial_Eurolite(); break;
		default: break;
		}
	}
}

// ============================================================================
// DMX Input (Enttec Pro)
// ============================================================================

void DMXUSBDevice::serialDataReceived(const var& data)
{
	if (activeMode != USBDMX_ENTTEC_PRO || !inputCC->enabled->boolValue())
		return;

	serialBuffer.addArray((const uint8*)data.getBinaryData()->getData(), (int)data.getBinaryData()->getSize());

	int endIndex = 0;
	Array<uint8> packet = getEnttecProPacket(serialBuffer, endIndex);
	while (packet.size() > 0)
	{
		processEnttecProPacket(packet);
		serialBuffer.removeRange(0, endIndex);
		packet = getEnttecProPacket(serialBuffer, endIndex);
	}
}

Array<uint8> DMXUSBDevice::getEnttecProPacket(Array<uint8>& buffer, int& endIndex)
{
	if (buffer.size() < 5) return Array<uint8>();

	int numBytes = buffer.size();
	for (int i = 0; i < numBytes; ++i)
	{
		if (buffer[i] == USB_DMX_START_MSG)
		{
			if (i + 3 >= numBytes) break;
			int length = (int)buffer[i + 2] + ((int)buffer[i + 3] << 8);
			if (buffer.size() - i < 4 + length) continue;
			endIndex = i + 4 + length;

			if (endIndex < numBytes && buffer[endIndex] == USB_DMX_END_MSG)
			{
				return Array<uint8>(buffer.getRawDataPointer() + i, 4 + length);
			}
		}
	}

	return Array<uint8>();
}

void DMXUSBDevice::processEnttecProPacket(const Array<uint8>& packet)
{
	if (packet.size() < 5) return;

	int label = (int)packet[1];
	int length = (int)packet[2] + ((int)packet[3] << 8);

	if (label == USB_DMX_RECV_LABEL && length > 1)
	{
		// Skip start code byte
		int numChannels = length - 1;
		if (numChannels > 512) numChannels = 512;
		setDMXValuesIn(numChannels, (uint8*)(packet.getRawDataPointer() + 5));
	}
}

void DMXUSBDevice::parseEnttecProInput(const uint8* data, int numBytes)
{
	serialBuffer.addArray((const uint8*)data, numBytes);

	int endIndex = 0;
	Array<uint8> packet = getEnttecProPacket(serialBuffer, endIndex);
	while (packet.size() > 0)
	{
		processEnttecProPacket(packet);
		serialBuffer.removeRange(0, endIndex);
		packet = getEnttecProPacket(serialBuffer, endIndex);
	}
}

// ============================================================================
// SerialManager Listener
// ============================================================================

void DMXUSBDevice::portAdded(SerialDeviceInfo* info)
{
	refreshDeviceList();

	// Auto-reconnect if this was our last opened port
	if (serialPort == nullptr && lastOpenedPortID.isNotEmpty() && info->port == lastOpenedPortID)
	{
		String selectedID = deviceSelector->getValueData().toString();
		if (selectedID != "none" && selectedID.isNotEmpty())
		{
			openConnection();
		}
	}
}

void DMXUSBDevice::portRemoved(SerialDeviceInfo* info)
{
	refreshDeviceList();
}

void DMXUSBDevice::portOpened(SerialDevice*) {}
void DMXUSBDevice::portClosed(SerialDevice*) {}

void DMXUSBDevice::portRemoved(SerialDevice*)
{
	serialPort = nullptr;
	setConnected(false);
}

// ============================================================================
// Windows Implementation
// ============================================================================

#elif JUCE_WINDOWS

DMXUSBDevice::DMXUSBDevice() :
	DMXDevice("USB DMX", USB_DMX, true),
	activeMode(USBDMX_OPEN_DMX),
	ftdiHandle(NULL),
	ftdiIsOpen(false),
	ftdiDefaultLatency(16),
	serialPort(nullptr)
{
	inputCC->enabled->setValue(false);

	deviceSelector = addEnumParameter("Device", "Select the USB DMX device to use");
	deviceSelector->addOption("None", "none");

	modeSelector = addEnumParameter("Mode", "Protocol mode. Auto detects from device name.");
	modeSelector->addOption("Auto", USBDMX_AUTO);
	modeSelector->addOption("Open DMX", USBDMX_OPEN_DMX);
	modeSelector->addOption("Enttec Pro", USBDMX_ENTTEC_PRO);
	modeSelector->addOption("DMXKing", USBDMX_DMXKING);
	modeSelector->addOption("Eurolite", USBDMX_EUROLITE);
	modeSelector->setValueWithKey("Auto");

	outputPortSelector = addEnumParameter("Output Port", "DMXKing output port");
	outputPortSelector->addOption("Port A", 1);
	outputPortSelector->hideInEditor = true;
	dmxKingPortCount = 1;

	SerialManager::getInstance()->addSerialManagerListener(this);
	refreshDeviceList();
	startTimer(2000);
}

DMXUSBDevice::~DMXUSBDevice()
{
	stopTimer();
	closeConnection();

	if (SerialManager::getInstanceWithoutCreating() != nullptr)
		SerialManager::getInstance()->removeSerialManagerListener(this);
}

// Auto-detection is shared with Mac/Linux (same static functions defined above)
// Redeclare here for Windows compilation unit

USBDMXMode DMXUSBDevice::autoDetectMode(const String& name, uint16 vid, uint16 pid)
{
	String upper = name.toUpperCase();

	if (upper.contains("PRO MK2") || upper.contains("PRO MK 2"))
		return USBDMX_ENTTEC_PRO;
	if (upper.contains("DMX USB PRO") || upper.contains("ENTTEC"))
		return USBDMX_ENTTEC_PRO;
	if (upper.contains("DMXKING") || upper.contains("DMX KING") || upper.contains("ULTRA"))
		return USBDMX_DMXKING;
	if (vid == 0x04D8 && pid == 0xFA63)
		return USBDMX_EUROLITE;
	if (upper.contains("EUROLITE"))
		return USBDMX_EUROLITE;
	return USBDMX_OPEN_DMX;
}

String DMXUSBDevice::modeToString(USBDMXMode mode)
{
	switch (mode)
	{
	case USBDMX_OPEN_DMX:   return "Open DMX";
	case USBDMX_ENTTEC_PRO: return "Enttec Pro";
	case USBDMX_DMXKING:    return "DMXKing";
	case USBDMX_EUROLITE:   return "Eurolite";
	default:                 return "Auto";
	}
}

void DMXUSBDevice::refreshDeviceList()
{
	String previousID = deviceSelector->getValueData().toString();

	detectedDevices.clear();
	deviceSelector->clearOptions();
	deviceSelector->addOption("None", "none");

	// 1. Enumerate FTDI devices
	StringArray ftdiSerials;
	Array<FTDIDeviceInfo> ftdiDevs = DMXOpenUSBFTDIDevice::enumerateDevices();
	for (const auto& fd : ftdiDevs)
	{
		DetectedUSBDevice d;
		d.name = fd.name;
		d.serial = fd.serial;
		d.portPath = "";
		d.ftdiIndex = (int)fd.id;
		d.vid = fd.vid;
		d.pid = fd.pid;
		d.isFTDI = true;
		d.detectedMode = autoDetectMode(fd.name, fd.vid, fd.pid);
		detectedDevices.add(d);
		ftdiSerials.add(fd.serial);
	}

	// Update hot-swap tracking
	lastFTDISerials = ftdiSerials;

	// 2. Enumerate serial devices (skip FTDI VID)
	for (auto* info : SerialManager::getInstance()->portInfos)
	{
		if (info->vid == 0 && info->pid == 0)
			continue;
		if (info->vid == FTDI_VID)
			continue;

		DetectedUSBDevice d;
		d.name = info->description;
		d.serial = info->hardwareID;
		d.portPath = info->port;
		d.ftdiIndex = -1;
		d.vid = (uint16)info->vid;
		d.pid = (uint16)info->pid;
		d.isFTDI = false;
		d.detectedMode = autoDetectMode(info->description, d.vid, d.pid);
		detectedDevices.add(d);
	}

	// 3. Populate dropdown
	for (const auto& d : detectedDevices)
	{
		String label = d.getDisplayLabel() + " [" + modeToString(d.detectedMode) + (d.isFTDI ? ", FTDI" : ", Serial") + "]";
		deviceSelector->addOption(label, d.getUniqueID());
	}

	if (previousID.isNotEmpty() && previousID != "none")
	{
		deviceSelector->setValueWithData(previousID);
	}
}

void DMXUSBDevice::onContainerParameterChanged(Parameter* p)
{
	DMXDevice::onContainerParameterChanged(p);

	if (p == deviceSelector)
	{
		String selectedID = deviceSelector->getValueData().toString();

		// Skip if already connected to this device
		if (selectedID == currentDevice.getUniqueID() &&
			(ftdiIsOpen || serialPort != nullptr))
			return;

		closeConnection();

		if (selectedID == "none" || selectedID.isEmpty())
			return;

		for (const auto& d : detectedDevices)
		{
			if (d.getUniqueID() == selectedID)
			{
				currentDevice = d;
				break;
			}
		}

		USBDMXMode selectedMode = (USBDMXMode)(int)modeSelector->getValueData();
		if (selectedMode == USBDMX_AUTO)
			activeMode = currentDevice.detectedMode;
		else
			activeMode = selectedMode;

		openConnection();
	}
	else if (p == modeSelector)
	{
		String selectedID = deviceSelector->getValueData().toString();
		if (selectedID == "none" || selectedID.isEmpty())
			return;

		USBDMXMode selectedMode = (USBDMXMode)(int)modeSelector->getValueData();
		USBDMXMode newMode = (selectedMode == USBDMX_AUTO) ? currentDevice.detectedMode : selectedMode;

		if (newMode != activeMode)
		{
			closeConnection();
			activeMode = newMode;
			openConnection();
		}
	}
}

// ============================================================================
// Timer - FTDI Hot-Swap
// ============================================================================

void DMXUSBDevice::timerCallback()
{
	// Re-scan FTDI devices to detect hot-plug/unplug
	Array<FTDIDeviceInfo> ftdiDevs = DMXOpenUSBFTDIDevice::enumerateDevices();

	StringArray currentSerials;
	for (const auto& fd : ftdiDevs)
		currentSerials.add(fd.serial);

	if (currentSerials == lastFTDISerials)
		return;

	// FTDI device list changed
	lastFTDISerials = currentSerials;

	// If our FTDI device was unplugged, close the connection
	if (ftdiIsOpen && currentDevice.isFTDI && !currentSerials.contains(currentDevice.serial))
	{
		NLOG(niceName, "FTDI device disconnected: " + currentDevice.serial);
		closeConnection();
	}

	refreshDeviceList();

	// If we have a selection but aren't connected, try to reconnect
	String selectedID = deviceSelector->getValueData().toString();
	if (selectedID != "none" && !selectedID.isEmpty() &&
		!ftdiIsOpen && serialPort == nullptr)
	{
		for (const auto& d : detectedDevices)
		{
			if (d.getUniqueID() == selectedID)
			{
				currentDevice = d;
				USBDMXMode selectedMode = (USBDMXMode)(int)modeSelector->getValueData();
				activeMode = (selectedMode == USBDMX_AUTO) ? currentDevice.detectedMode : selectedMode;
				openConnection();
				break;
			}
		}
	}
}

void DMXUSBDevice::openConnection()
{
	if (currentDevice.isFTDI)
	{
		if (!openFTDI(currentDevice.ftdiIndex))
			return;

		// If auto-detect guessed Enttec Pro, probe to see if it's actually a DMXKing
		USBDMXMode selectedMode = (USBDMXMode)(int)modeSelector->getValueData();
		if (selectedMode == USBDMX_AUTO && activeMode == USBDMX_ENTTEC_PRO && probeDMXKingFTDI())
		{
			NLOG(niceName, "Device identified as DMXKing via firmware probe");
			activeMode = USBDMX_DMXKING;
			currentDevice.detectedMode = USBDMX_DMXKING;
			updateDeviceLabel(currentDevice.getUniqueID(), USBDMX_DMXKING);
		}

		// If DMXKing (auto-detected or manually selected), probe port count
		if (activeMode == USBDMX_DMXKING)
			probeDMXKingFTDI();

		updateOutputPortVisibility();
	}
	else if (currentDevice.portPath.isNotEmpty())
	{
		openSerial(currentDevice.portPath);
		updateOutputPortVisibility();
	}
}

void DMXUSBDevice::closeConnection()
{
	closeFTDI();
	closeSerial();
	serialBuffer.clear();
	updateOutputPortVisibility();
}

bool DMXUSBDevice::openFTDI(int deviceIndex)
{
	if (ftdiIsOpen)
		closeFTDI();

	FT_STATUS status = FT_Open(deviceIndex, &ftdiHandle);
	if (status != FT_OK)
	{
		LOGWARNING("USB DMX: Could not open FTDI device index " + String(deviceIndex));
		return false;
	}

	if (FT_GetLatencyTimer(ftdiHandle, &ftdiDefaultLatency) != FT_OK)
		ftdiDefaultLatency = 16;

	if (FT_ResetDevice(ftdiHandle) != FT_OK)
	{
		LOGWARNING("USB DMX: FTDI reset failed");
		FT_Close(ftdiHandle);
		ftdiHandle = NULL;
		return false;
	}

	DWORD baudRate = 250000;
	UCHAR stopBits = FT_STOP_BITS_2;

	if (activeMode == USBDMX_ENTTEC_PRO || activeMode == USBDMX_DMXKING)
	{
		baudRate = 115200;
		stopBits = FT_STOP_BITS_1;
	}

	if (FT_SetBaudRate(ftdiHandle, baudRate) != FT_OK)
	{
		LOGWARNING("USB DMX: FTDI set baud rate failed");
		FT_Close(ftdiHandle);
		ftdiHandle = NULL;
		return false;
	}

	if (FT_SetDataCharacteristics(ftdiHandle, FT_BITS_8, stopBits, FT_PARITY_NONE) != FT_OK)
	{
		LOGWARNING("USB DMX: FTDI set data characteristics failed");
		FT_Close(ftdiHandle);
		ftdiHandle = NULL;
		return false;
	}

	if (FT_SetFlowControl(ftdiHandle, FT_FLOW_NONE, 0, 0) != FT_OK)
	{
		LOGWARNING("USB DMX: FTDI set flow control failed");
		FT_Close(ftdiHandle);
		ftdiHandle = NULL;
		return false;
	}

	FT_SetLatencyTimer(ftdiHandle, 1);
	FT_ClrRts(ftdiHandle);
	FT_Purge(ftdiHandle, FT_PURGE_RX | FT_PURGE_TX);

	ftdiIsOpen = true;
	NLOG(niceName, "FTDI device opened (index " + String(deviceIndex) + ") mode: " + modeToString(activeMode));
	setConnected(true);
	return true;
}

void DMXUSBDevice::closeFTDI()
{
	if (!ftdiIsOpen)
		return;

	FT_SetLatencyTimer(ftdiHandle, ftdiDefaultLatency);
	FT_Close(ftdiHandle);
	ftdiHandle = NULL;
	ftdiIsOpen = false;
	setConnected(false);
}

void DMXUSBDevice::sendFTDI_OpenDMX()
{
	if (!ftdiIsOpen) return;

	FT_SetBreakOn(ftdiHandle);
	Sleep(1);
	FT_SetBreakOff(ftdiHandle);
	Sleep(1);

	uint8 dmxFrame[513];
	dmxFrame[0] = 0x00;
	memcpy(dmxFrame + 1, dmxDataOut, 512);

	DWORD written = 0;
	FT_Write(ftdiHandle, dmxFrame, 513, &written);
}

void DMXUSBDevice::sendFTDI_EnttecPro()
{
	if (!ftdiIsOpen) return;

	uint16 len = 513;
	uint8 header[5] = { USB_DMX_START_MSG, USB_DMX_SEND_LABEL, (uint8)(len & 0xFF), (uint8)(len >> 8), 0x00 };
	uint8 footer[1] = { USB_DMX_END_MSG };

	DWORD written = 0;
	FT_Write(ftdiHandle, header, 5, &written);
	FT_Write(ftdiHandle, dmxDataOut, 512, &written);
	FT_Write(ftdiHandle, footer, 1, &written);
}

void DMXUSBDevice::sendFTDI_DMXKing()
{
	if (!ftdiIsOpen) return;

	int port = (int)outputPortSelector->getValueData();
	uint8 sendLabel = DMXKING_SEND_PORT1_LABEL + (port - 1);

	uint16 len = 513;
	uint8 header[5] = { USB_DMX_START_MSG, sendLabel, (uint8)(len & 0xFF), (uint8)(len >> 8), 0x00 };
	uint8 footer[1] = { USB_DMX_END_MSG };

	DWORD written = 0;
	FT_Write(ftdiHandle, header, 5, &written);
	FT_Write(ftdiHandle, dmxDataOut, 512, &written);
	FT_Write(ftdiHandle, footer, 1, &written);
}

void DMXUSBDevice::sendFTDI_Eurolite()
{
	if (!ftdiIsOpen) return;

	FT_SetBreakOn(ftdiHandle);
	Sleep(1);
	FT_SetBreakOff(ftdiHandle);
	Sleep(1);

	uint16 len = 513;
	uint8 header[5] = { USB_DMX_START_MSG, USB_DMX_SEND_LABEL, (uint8)(len & 0xFF), (uint8)(len >> 8), 0x00 };
	uint8 footer[1] = { USB_DMX_END_MSG };

	DWORD written = 0;
	FT_Write(ftdiHandle, header, 5, &written);
	FT_Write(ftdiHandle, dmxDataOut, 512, &written);
	FT_Write(ftdiHandle, footer, 1, &written);
}

bool DMXUSBDevice::openSerial(const String& portPath)
{
	closeSerial();

	SerialDeviceInfo* info = nullptr;
	for (auto* pi : SerialManager::getInstance()->portInfos)
	{
		if (pi->port == portPath)
		{
			info = pi;
			break;
		}
	}

	if (info == nullptr) return false;

	serialPort = SerialManager::getInstance()->getPort(info);
	if (serialPort == nullptr) return false;

	serialPort->addSerialDeviceListener(this);
	serialPort->setMode(SerialDevice::PortMode::RAW);
	serialPort->open();

	if (!serialPort->isOpen())
	{
		NLOG(niceName, "Could not open serial port: " + portPath);
		serialPort->removeSerialDeviceListener(this);
		serialPort = nullptr;
		return false;
	}

	lastOpenedPortID = portPath;

	switch (activeMode)
	{
	case USBDMX_OPEN_DMX:   configureSerialPort_OpenDMX(); break;
	case USBDMX_ENTTEC_PRO: configureSerialPort_EnttecPro(); break;
	case USBDMX_DMXKING:    configureSerialPort_EnttecPro(); break;
	case USBDMX_EUROLITE:   configureSerialPort_Eurolite(); break;
	default: break;
	}

	NLOG(niceName, "Serial port opened: " + portPath + " mode: " + modeToString(activeMode));
	setConnected(true);
	return true;
}

void DMXUSBDevice::closeSerial()
{
	if (serialPort != nullptr)
	{
		serialPort->removeSerialDeviceListener(this);
		serialPort = nullptr;
		setConnected(false);
	}
}

void DMXUSBDevice::configureSerialPort_OpenDMX()
{
	if (serialPort == nullptr || serialPort->port == nullptr) return;
	try
	{
		serialPort->port->setBaudrate(250000);
		serialPort->port->setBytesize(serial::eightbits);
		serialPort->port->setStopbits(serial::stopbits_two);
		serialPort->port->setParity(serial::parity_none);
		serialPort->port->setFlowcontrol(serial::flowcontrol_none);
		serialPort->port->setRTS(false);
		serialPort->port->setDTR(false);
		serialPort->port->flush();
	}
	catch (serial::IOException e)
	{
		LOGERROR("USB DMX: Error configuring Open DMX port: " << e.what());
	}
}

void DMXUSBDevice::configureSerialPort_EnttecPro()
{
	if (serialPort == nullptr || serialPort->port == nullptr) return;
	try
	{
		serialPort->port->setBaudrate(115200);
		serialPort->port->setBytesize(serial::eightbits);
		serialPort->port->setStopbits(serial::stopbits_one);
		serialPort->port->setParity(serial::parity_none);
		serialPort->port->flush();

		uint8 getSerialCmd[5] = { USB_DMX_START_MSG, (uint8)USB_DMX_SERIAL_NUMBER_LABEL, 0, 0, USB_DMX_END_MSG };
		serialPort->port->write(getSerialCmd, 5);

		uint8 changeAlways[6] = { USB_DMX_START_MSG, (uint8)USB_DMX_RECV_ON_CHANGE_LABEL, 1, 0, 0, USB_DMX_END_MSG };
		serialPort->port->write(changeAlways, 6);
	}
	catch (serial::IOException e)
	{
		LOGERROR("USB DMX: Error configuring Enttec Pro port: " << e.what());
	}
}

void DMXUSBDevice::configureSerialPort_Eurolite()
{
	if (serialPort == nullptr || serialPort->port == nullptr) return;
	try
	{
		serialPort->port->setBaudrate(250000);
		serialPort->port->setBytesize(serial::eightbits);
		serialPort->port->setStopbits(serial::stopbits_two);
		serialPort->port->setParity(serial::parity_none);
		serialPort->port->setFlowcontrol(serial::flowcontrol_none);
		serialPort->port->setRTS(false);
		serialPort->port->setDTR(false);
		serialPort->port->flush();
	}
	catch (serial::IOException e)
	{
		LOGERROR("USB DMX: Error configuring Eurolite port: " << e.what());
	}
}

void DMXUSBDevice::sendSerial_OpenDMX()
{
	if (serialPort == nullptr || !serialPort->isOpen()) return;
	try
	{
		serialPort->port->setBreak(true);
		serialPort->port->setBreak(false);
		uint8 startCode = 0x00;
		serialPort->port->write(&startCode, 1);
		serialPort->port->write(dmxDataOut, 512);
	}
	catch (std::exception& e)
	{
		DBG("USB DMX Serial OpenDMX send error: " << e.what());
	}
}

void DMXUSBDevice::sendSerial_EnttecPro()
{
	if (serialPort == nullptr || !serialPort->isOpen()) return;
	try
	{
		uint16 len = 513;
		uint8 header[5] = { USB_DMX_START_MSG, USB_DMX_SEND_LABEL, (uint8)(len & 0xFF), (uint8)(len >> 8), 0x00 };
		uint8 footer[1] = { USB_DMX_END_MSG };

		serialPort->port->write(header, 5);
		serialPort->port->write(dmxDataOut, 512);
		serialPort->port->write(footer, 1);
		serialPort->port->flush();

		if (inputCC->enabled->boolValue())
		{
			uint8 changeAlways[6] = { USB_DMX_START_MSG, (uint8)USB_DMX_RECV_ON_CHANGE_LABEL, 1, 0, 0, USB_DMX_END_MSG };
			serialPort->port->write(changeAlways, 6);
		}
	}
	catch (std::exception& e)
	{
		DBG("USB DMX Serial EnttecPro send error: " << e.what());
	}
}

void DMXUSBDevice::sendSerial_DMXKing()
{
	if (serialPort == nullptr || !serialPort->isOpen()) return;
	try
	{
		int port = (int)outputPortSelector->getValueData();
		uint8 sendLabel = DMXKING_SEND_PORT1_LABEL + (port - 1);

		uint16 len = 513;
		uint8 header[5] = { USB_DMX_START_MSG, sendLabel, (uint8)(len & 0xFF), (uint8)(len >> 8), 0x00 };
		uint8 footer[1] = { USB_DMX_END_MSG };

		serialPort->port->write(header, 5);
		serialPort->port->write(dmxDataOut, 512);
		serialPort->port->write(footer, 1);
		serialPort->port->flush();
	}
	catch (std::exception& e)
	{
		DBG("USB DMX Serial DMXKing send error: " << e.what());
	}
}

void DMXUSBDevice::sendSerial_Eurolite()
{
	if (serialPort == nullptr || !serialPort->isOpen()) return;
	try
	{
		serialPort->port->setBreak(true);
		serialPort->port->setBreak(false);

		uint16 len = 513;
		uint8 header[5] = { USB_DMX_START_MSG, USB_DMX_SEND_LABEL, (uint8)(len & 0xFF), (uint8)(len >> 8), 0x00 };
		uint8 footer[1] = { USB_DMX_END_MSG };

		serialPort->port->write(header, 5);
		serialPort->port->write(dmxDataOut, 512);
		serialPort->port->write(footer, 1);
		serialPort->port->flush();
	}
	catch (std::exception& e)
	{
		DBG("USB DMX Serial Eurolite send error: " << e.what());
	}
}

bool DMXUSBDevice::probeDMXKingFTDI()
{
	if (!ftdiIsOpen) return false;

	// Send DMXKing port count query (label 0x63)
	uint8 probe[5] = { 0x7E, 0x63, 0x00, 0x00, 0xE7 };
	DWORD written = 0;
	if (FT_Write(ftdiHandle, probe, 5, &written) != FT_OK || written < 5)
	{
		NLOG(niceName, "DMXKing probe: write failed");
		return false;
	}

	// Try a few reads with short waits
	for (int attempt = 0; attempt < 5; attempt++)
	{
		Sleep(100);

		DWORD rxBytes = 0;
		FT_GetQueueStatus(ftdiHandle, &rxBytes);

		if (rxBytes > 1)
		{
			uint8 buf[64];
			DWORD bytesRead = 0;
			FT_Read(ftdiHandle, buf, jmin((DWORD)64, rxBytes), &bytesRead);

			for (DWORD i = 0; i + 1 < bytesRead; i++)
			{
				if (buf[i] == 0x7E && buf[i + 1] == 0x63)
				{
					// Extract port count from response
					// Response format: 0x7E 0x63 [len_lo] [len_hi] [port_count] ... 0xE7
					if (i + 4 < bytesRead)
					{
						int portCount = (int)buf[i + 4];
						if (portCount >= 1 && portCount <= 8)
							dmxKingPortCount = portCount;
					}

					NLOG(niceName, "DMXKing probe: positive response");
					return true;
				}
			}
		}
	}

	NLOG(niceName, "DMXKing probe: no response (device is likely Enttec Pro compatible)");
	return false;
}

void DMXUSBDevice::updateDeviceLabel(const String& uniqueID, USBDMXMode newMode)
{
	for (int i = 0; i < detectedDevices.size(); i++)
	{
		if (detectedDevices[i].getUniqueID() == uniqueID)
		{
			detectedDevices.getReference(i).detectedMode = newMode;
			break;
		}
	}

	// Rebuild dropdown options with updated label
	String previousID = deviceSelector->getValueData().toString();
	deviceSelector->clearOptions();
	deviceSelector->addOption("None", "none");

	for (const auto& d : detectedDevices)
	{
		String label = d.getDisplayLabel() + " [" + modeToString(d.detectedMode) + (d.isFTDI ? ", FTDI" : ", Serial") + "]";
		deviceSelector->addOption(label, d.getUniqueID());
	}

	if (previousID.isNotEmpty() && previousID != "none")
		deviceSelector->setValueWithData(previousID);
}

void DMXUSBDevice::updateOutputPortVisibility()
{
	bool showPorts = (activeMode == USBDMX_DMXKING);

	if (showPorts)
	{
		// Rebuild port options based on detected count
		String previousPort = outputPortSelector->getValueData().toString();
		outputPortSelector->clearOptions();

		const char* portNames[] = { "Port A", "Port B", "Port C", "Port D", "Port E", "Port F", "Port G", "Port H" };
		for (int i = 0; i < dmxKingPortCount && i < 8; i++)
			outputPortSelector->addOption(portNames[i], i + 1);

		if (previousPort.isNotEmpty())
			outputPortSelector->setValueWithData(previousPort);
	}

	outputPortSelector->hideInEditor = !showPorts;
	queuedNotifier.addMessage(new ContainerAsyncEvent(ContainerAsyncEvent::ControllableContainerNeedsRebuild, this));
}

void DMXUSBDevice::sendDMXValuesInternal()
{
	if (ftdiIsOpen)
	{
		switch (activeMode)
		{
		case USBDMX_OPEN_DMX:   sendFTDI_OpenDMX(); break;
		case USBDMX_ENTTEC_PRO: sendFTDI_EnttecPro(); break;
		case USBDMX_DMXKING:    sendFTDI_DMXKing(); break;
		case USBDMX_EUROLITE:   sendFTDI_Eurolite(); break;
		default: break;
		}
	}
	else if (serialPort != nullptr && serialPort->isOpen())
	{
		switch (activeMode)
		{
		case USBDMX_OPEN_DMX:   sendSerial_OpenDMX(); break;
		case USBDMX_ENTTEC_PRO: sendSerial_EnttecPro(); break;
		case USBDMX_DMXKING:    sendSerial_DMXKing(); break;
		case USBDMX_EUROLITE:   sendSerial_Eurolite(); break;
		default: break;
		}
	}
}

void DMXUSBDevice::serialDataReceived(const var& data)
{
	if (activeMode != USBDMX_ENTTEC_PRO || !inputCC->enabled->boolValue())
		return;

	serialBuffer.addArray((const uint8*)data.getBinaryData()->getData(), (int)data.getBinaryData()->getSize());

	int endIndex = 0;
	Array<uint8> packet = getEnttecProPacket(serialBuffer, endIndex);
	while (packet.size() > 0)
	{
		processEnttecProPacket(packet);
		serialBuffer.removeRange(0, endIndex);
		packet = getEnttecProPacket(serialBuffer, endIndex);
	}
}

Array<uint8> DMXUSBDevice::getEnttecProPacket(Array<uint8>& buffer, int& endIndex)
{
	if (buffer.size() < 5) return Array<uint8>();

	int numBytes = buffer.size();
	for (int i = 0; i < numBytes; ++i)
	{
		if (buffer[i] == USB_DMX_START_MSG)
		{
			if (i + 3 >= numBytes) break;
			int length = (int)buffer[i + 2] + ((int)buffer[i + 3] << 8);
			if (buffer.size() - i < 4 + length) continue;
			endIndex = i + 4 + length;

			if (endIndex < numBytes && buffer[endIndex] == USB_DMX_END_MSG)
			{
				return Array<uint8>(buffer.getRawDataPointer() + i, 4 + length);
			}
		}
	}

	return Array<uint8>();
}

void DMXUSBDevice::processEnttecProPacket(const Array<uint8>& packet)
{
	if (packet.size() < 5) return;

	int label = (int)packet[1];
	int length = (int)packet[2] + ((int)packet[3] << 8);

	if (label == USB_DMX_RECV_LABEL && length > 1)
	{
		int numChannels = length - 1;
		if (numChannels > 512) numChannels = 512;
		setDMXValuesIn(numChannels, (uint8*)(packet.getRawDataPointer() + 5));
	}
}

void DMXUSBDevice::parseEnttecProInput(const uint8* data, int numBytes)
{
	serialBuffer.addArray((const uint8*)data, numBytes);

	int endIndex = 0;
	Array<uint8> packet = getEnttecProPacket(serialBuffer, endIndex);
	while (packet.size() > 0)
	{
		processEnttecProPacket(packet);
		serialBuffer.removeRange(0, endIndex);
		packet = getEnttecProPacket(serialBuffer, endIndex);
	}
}

void DMXUSBDevice::portAdded(SerialDeviceInfo* info)
{
	refreshDeviceList();

	if (serialPort == nullptr && lastOpenedPortID.isNotEmpty() && info->port == lastOpenedPortID)
	{
		String selectedID = deviceSelector->getValueData().toString();
		if (selectedID != "none" && selectedID.isNotEmpty())
		{
			openConnection();
		}
	}
}

void DMXUSBDevice::portRemoved(SerialDeviceInfo* info)
{
	refreshDeviceList();
}

void DMXUSBDevice::portOpened(SerialDevice*) {}
void DMXUSBDevice::portClosed(SerialDevice*) {}

void DMXUSBDevice::portRemoved(SerialDevice*)
{
	serialPort = nullptr;
	setConnected(false);
}

#else

// Stub implementation for unsupported platforms
DMXUSBDevice::DMXUSBDevice() :
	DMXDevice("USB DMX", USB_DMX, false)
{
	LOGWARNING("USB DMX is not supported on this platform");
}

DMXUSBDevice::~DMXUSBDevice()
{
}

void DMXUSBDevice::sendDMXValuesInternal()
{
}

#endif
