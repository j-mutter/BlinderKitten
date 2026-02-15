/*
  ==============================================================================

    DMXKingFTDIManager.cpp
    Created: 15 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#include "DMXKingFTDIManager.h"
#include "DMXKingFTDIDevice.h"
#include "DMXOpenUSBFTDIDevice.h"

#if JUCE_MAC || JUCE_LINUX

juce_ImplementSingleton(DMXKingFTDIManager)

DMXKingFTDIManager::DMXKingFTDIManager() :
	Thread("DMXKing FTDI Manager")
{
}

DMXKingFTDIManager::~DMXKingFTDIManager()
{
	stopThread(500);
	for (auto* hw : hardware)
	{
		closeHardware(hw);
	}
	hardware.clear();
}

DMXKingFTDIManager::SharedFTDIHardware* DMXKingFTDIManager::findHardware(const String& serial)
{
	for (auto* hw : hardware)
	{
		if (hw->serial == serial)
			return hw;
	}
	return nullptr;
}

void DMXKingFTDIManager::registerDevice(DMXKingFTDIDevice* device, const String& serial)
{
	SharedFTDIHardware* hw = findHardware(serial);

	if (hw == nullptr)
	{
		hw = new SharedFTDIHardware();
		hardware.add(hw);

		if (!openHardware(hw, serial))
		{
			hardware.removeObject(hw);
			return;
		}
	}

	hw->connectedDevices.addIfNotAlreadyThere(device);

	if (hw->outputPortCountDetected)
	{
		device->onPortCountDetected(hw->detectedOutputPortCount);
	}

	if (!isThreadRunning())
		startThread();
}

void DMXKingFTDIManager::unregisterDevice(DMXKingFTDIDevice* device)
{
	for (int i = hardware.size() - 1; i >= 0; --i)
	{
		auto* hw = hardware[i];
		hw->connectedDevices.removeAllInstancesOf(device);

		if (hw->connectedDevices.isEmpty())
		{
			closeHardware(hw);
			hardware.remove(i);
		}
	}

	// Stop thread if no hardware left
	bool anyHardware = hardware.size() > 0;
	if (!anyHardware && isThreadRunning())
		stopThread(500);
}

bool DMXKingFTDIManager::openHardware(SharedFTDIHardware* hw, const String& serial)
{
	const char* ser = serial.isNotEmpty() ? serial.toRawUTF8() : nullptr;

	if (ftdi_usb_open_desc(&hw->ftdi, DMXOpenUSBFTDIDevice::FTDI_VID, DMXOpenUSBFTDIDevice::FTDI_PID, nullptr, ser) < 0)
	{
		if (ftdi_usb_open_desc(&hw->ftdi, DMXOpenUSBFTDIDevice::FTDI_VID, DMXOpenUSBFTDIDevice::FTDI2_PID, nullptr, ser) < 0)
		{
			LOGWARNING("DMXKing FTDI: Could not open device: " + String(ftdi_get_error_string(&hw->ftdi)));
			return false;
		}
	}

	if (ftdi_usb_reset(&hw->ftdi) < 0)
	{
		LOGWARNING("DMXKing FTDI: Reset failed: " + String(ftdi_get_error_string(&hw->ftdi)));
		ftdi_usb_close(&hw->ftdi);
		return false;
	}

	if (ftdi_set_baudrate(&hw->ftdi, 115200) < 0)
	{
		LOGWARNING("DMXKing FTDI: Set baud rate failed: " + String(ftdi_get_error_string(&hw->ftdi)));
		ftdi_usb_close(&hw->ftdi);
		return false;
	}

	if (ftdi_set_line_property(&hw->ftdi, BITS_8, STOP_BIT_1, NONE) < 0)
	{
		LOGWARNING("DMXKing FTDI: Set line property failed: " + String(ftdi_get_error_string(&hw->ftdi)));
		ftdi_usb_close(&hw->ftdi);
		return false;
	}

	if (ftdi_setflowctrl(&hw->ftdi, SIO_DISABLE_FLOW_CTRL) < 0)
	{
		LOGWARNING("DMXKing FTDI: Set flow control failed: " + String(ftdi_get_error_string(&hw->ftdi)));
		ftdi_usb_close(&hw->ftdi);
		return false;
	}

	if (ftdi_set_latency_timer(&hw->ftdi, 1) < 0)
	{
		DBG("DMXKing FTDI: Could not set low latency, using default");
	}

#if defined(LIBFTDI1_5)
	if (ftdi_tcioflush(&hw->ftdi) < 0)
#else
	if (ftdi_usb_purge_buffers(&hw->ftdi) < 0)
#endif
	{
		DBG("DMXKing FTDI: Could not purge buffers");
	}

	hw->serial = serial;
	NLOG("DMXKing FTDI", "Device opened: " + serial);

	// Poll capabilities
	pollDeviceCapabilities(hw);

	return true;
}

void DMXKingFTDIManager::closeHardware(SharedFTDIHardware* hw)
{
	if (hw->serial.isEmpty())
		return;

	ftdi_set_latency_timer(&hw->ftdi, 16);
	ftdi_usb_close(&hw->ftdi);
	NLOG("DMXKing FTDI", "Device closed: " + hw->serial);
	hw->serial = "";

	for (auto* device : hw->connectedDevices)
	{
		device->onSharedHardwareDisconnected();
	}
}

void DMXKingFTDIManager::sendDMXData(const String& serial, int outputPort, const uint8* data)
{
	SharedFTDIHardware* hw = findHardware(serial);
	if (hw == nullptr || hw->serial.isEmpty())
		return;

	const ScopedLock sl(hw->lock);

	uint8 outputPortLabel = DMXKING_SEND_PORT1_LABEL + (outputPort - 1);

	uint8 header[5] = {
		DMXKING_START_MESSAGE,
		outputPortLabel,
		(uint8)((DMXKING_CHANNEL_COUNT + 1) & 0xFF),
		(uint8)(((DMXKING_CHANNEL_COUNT + 1) >> 8) & 0xFF),
		DMXKING_START_CODE
	};

	uint8 footer[1] = { DMXKING_END_MESSAGE };

	// Write as single buffer for efficiency
	uint8 frame[518]; // 5 header + 512 data + 1 footer
	memcpy(frame, header, 5);
	memcpy(frame + 5, data, 512);
	frame[517] = DMXKING_END_MESSAGE;

	if (ftdi_write_data(&hw->ftdi, frame, 518) < 0)
	{
		DBG("DMXKing FTDI: Write failed: " + String(ftdi_get_error_string(&hw->ftdi)));
	}
}

void DMXKingFTDIManager::pollDeviceCapabilities(SharedFTDIHardware* hw)
{
	const ScopedLock sl(hw->lock);

	uint8 request[5] = { DMXKING_START_MESSAGE, DMXKING_DMX_PORT_COUNT_LABEL, 0, 0, DMXKING_END_MESSAGE };

	if (ftdi_write_data(&hw->ftdi, request, 5) < 0)
	{
		DBG("DMXKing FTDI: Failed to send port count request: " + String(ftdi_get_error_string(&hw->ftdi)));
	}
	else
	{
		LOG("DMXKing FTDI: Polling port count for " + hw->serial);
	}
}

void DMXKingFTDIManager::run()
{
	while (!threadShouldExit())
	{
		for (auto* hw : hardware)
		{
			if (hw->serial.isEmpty())
				continue;

			processReadData(hw);
		}

		Thread::sleep(10);
	}
}

void DMXKingFTDIManager::processReadData(SharedFTDIHardware* hw)
{
	const ScopedLock sl(hw->lock);

	uint8 buffer[1024];
	int bytesRead = ftdi_read_data(&hw->ftdi, buffer, sizeof(buffer));

	if (bytesRead > 0)
	{
		hw->readBuffer.addArray((const uint8*)buffer, bytesRead);

		int endIndex = 0;
		Array<uint8> packet = getDMXPacket(hw->readBuffer, endIndex);
		while (packet.size() > 0)
		{
			processDMXPacket(hw, packet);
			hw->readBuffer.removeRange(0, endIndex);
			packet = getDMXPacket(hw->readBuffer, endIndex);
		}
	}
}

Array<uint8> DMXKingFTDIManager::getDMXPacket(Array<uint8> bytes, int& endIndex)
{
	if (bytes.size() < DMXKING_HEADER_LENGTH + 1) return Array<uint8>();
	int numBytes = bytes.size();

	for (int i = 0; i < numBytes; ++i)
	{
		if (bytes[i] == DMXKING_START_MESSAGE)
		{
			if (i + 3 >= numBytes) break;
			int length = (int)bytes[i + 2] + ((int)bytes[i + 3] << 8);
			if (bytes.size() - i < DMXKING_HEADER_LENGTH + length) continue;
			endIndex = i + DMXKING_HEADER_LENGTH + length;

			if (endIndex < numBytes && bytes[endIndex] == DMXKING_END_MESSAGE)
			{
				endIndex++; // include end byte
				return Array<uint8>(bytes.getRawDataPointer() + i, DMXKING_HEADER_LENGTH + length);
			}
		}
	}

	return Array<uint8>();
}

void DMXKingFTDIManager::processDMXPacket(SharedFTDIHardware* hw, Array<uint8> bytes)
{
	int messageLabel = (int)bytes[1];
	int length = (int)bytes[2] + ((int)bytes[3] << 8);

	switch (messageLabel)
	{
	case DMXKING_RECEIVE_LABEL:
	{
		if (length > DMXKING_CHANNEL_COUNT + 2)
		{
			LOGWARNING("DMXKing FTDI: bad DMX Length: " << length);
			return;
		}

		if (length > bytes.size())
		{
			LOGWARNING("DMXKing FTDI: bad data size: " << bytes.size() << " <> " << length);
			return;
		}

		if (bytes[DMXKING_HEADER_LENGTH] != DMXKING_START_CODE)
		{
			LOGWARNING("DMXKing FTDI: bad start code " << bytes[DMXKING_HEADER_LENGTH] << " should be " << DMXKING_START_CODE);
			return;
		}

		for (auto* device : hw->connectedDevices)
		{
			if (device->inputCC && device->inputCC->enabled->boolValue())
			{
				device->setDMXValuesIn(length - 1, bytes.getRawDataPointer() + DMXKING_HEADER_LENGTH + 1);
			}
		}
	}
	break;

	case DMXKING_RECEIVE_SERIAL_NUMBER_LABEL:
	{
		if (length >= 4)
		{
			int serialNumber = (int)(bytes[4] + (bytes[5] << 8) + (bytes[6] << 16) + (bytes[7] << 24));
			LOG("DMXKing FTDI: Got serial number: " << String::toHexString(serialNumber));
		}
	}
	break;

	case DMXKING_DMX_PORT_COUNT_LABEL:
	{
		if (length >= 1)
		{
			int portCount = (int)bytes[4];
			hw->detectedOutputPortCount = portCount;
			hw->outputPortCountDetected = true;
			notifyDevicesOutputPortCountChanged(hw);
		}
		else
		{
			LOGWARNING("DMXKing FTDI: Invalid port count response length: " << length);
		}
	}
	break;

	default:
		break;
	}
}

void DMXKingFTDIManager::notifyDevicesOutputPortCountChanged(SharedFTDIHardware* hw)
{
	LOG("DMXKing FTDI: Detected " + String(hw->detectedOutputPortCount) + " output ports");

	for (auto* device : hw->connectedDevices)
	{
		device->onPortCountDetected(hw->detectedOutputPortCount);
	}
}

#elif JUCE_WINDOWS

juce_ImplementSingleton(DMXKingFTDIManager)

DMXKingFTDIManager::DMXKingFTDIManager() :
	Thread("DMXKing FTDI Manager")
{
}

DMXKingFTDIManager::~DMXKingFTDIManager()
{
	stopThread(500);
	for (auto* hw : hardware)
	{
		closeHardware(hw);
	}
	hardware.clear();
}

DMXKingFTDIManager::SharedFTDIHardware* DMXKingFTDIManager::findHardware(const String& serial)
{
	for (auto* hw : hardware)
	{
		if (hw->serial == serial)
			return hw;
	}
	return nullptr;
}

void DMXKingFTDIManager::registerDevice(DMXKingFTDIDevice* device, const String& serial)
{
	SharedFTDIHardware* hw = findHardware(serial);

	if (hw == nullptr)
	{
		hw = new SharedFTDIHardware();
		hardware.add(hw);

		// On Windows, find device index from serial
		Array<FTDIDeviceInfo> devices = DMXOpenUSBFTDIDevice::enumerateDevices();
		int deviceIndex = -1;
		for (const auto& dev : devices)
		{
			if (dev.serial == serial)
			{
				deviceIndex = (int)dev.id;
				break;
			}
		}

		if (deviceIndex < 0 || !openHardware(hw, deviceIndex))
		{
			hardware.removeObject(hw);
			return;
		}
	}

	hw->connectedDevices.addIfNotAlreadyThere(device);

	if (hw->outputPortCountDetected)
	{
		device->onPortCountDetected(hw->detectedOutputPortCount);
	}

	if (!isThreadRunning())
		startThread();
}

void DMXKingFTDIManager::unregisterDevice(DMXKingFTDIDevice* device)
{
	for (int i = hardware.size() - 1; i >= 0; --i)
	{
		auto* hw = hardware[i];
		hw->connectedDevices.removeAllInstancesOf(device);

		if (hw->connectedDevices.isEmpty())
		{
			closeHardware(hw);
			hardware.remove(i);
		}
	}

	bool anyHardware = hardware.size() > 0;
	if (!anyHardware && isThreadRunning())
		stopThread(500);
}

bool DMXKingFTDIManager::openHardware(SharedFTDIHardware* hw, int deviceIndex)
{
	FT_STATUS status = FT_Open(deviceIndex, &hw->handle);
	if (status != FT_OK)
	{
		LOGWARNING("DMXKing FTDI: Could not open device index " + String(deviceIndex) + " (status: " + String((int)status) + ")");
		return false;
	}

	if (FT_ResetDevice(hw->handle) != FT_OK)
	{
		LOGWARNING("DMXKing FTDI: Reset failed");
		FT_Close(hw->handle);
		hw->handle = NULL;
		return false;
	}

	if (FT_SetBaudRate(hw->handle, 115200) != FT_OK)
	{
		LOGWARNING("DMXKing FTDI: Set baud rate failed");
		FT_Close(hw->handle);
		hw->handle = NULL;
		return false;
	}

	if (FT_SetDataCharacteristics(hw->handle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE) != FT_OK)
	{
		LOGWARNING("DMXKing FTDI: Set data characteristics failed");
		FT_Close(hw->handle);
		hw->handle = NULL;
		return false;
	}

	if (FT_SetFlowControl(hw->handle, FT_FLOW_NONE, 0, 0) != FT_OK)
	{
		LOGWARNING("DMXKing FTDI: Set flow control failed");
		FT_Close(hw->handle);
		hw->handle = NULL;
		return false;
	}

	if (FT_SetLatencyTimer(hw->handle, 1) != FT_OK)
	{
		DBG("DMXKing FTDI: Could not set low latency, using default");
	}

	if (FT_Purge(hw->handle, FT_PURGE_RX | FT_PURGE_TX) != FT_OK)
	{
		DBG("DMXKing FTDI: Could not purge buffers");
	}

	hw->deviceIndex = deviceIndex;
	hw->serial = String(deviceIndex);
	NLOG("DMXKing FTDI", "Device opened (index " + String(deviceIndex) + ")");

	pollDeviceCapabilities(hw);

	return true;
}

void DMXKingFTDIManager::closeHardware(SharedFTDIHardware* hw)
{
	if (hw->handle == NULL)
		return;

	FT_SetLatencyTimer(hw->handle, 16);
	FT_Close(hw->handle);
	NLOG("DMXKing FTDI", "Device closed: " + hw->serial);
	hw->handle = NULL;
	hw->serial = "";

	for (auto* device : hw->connectedDevices)
	{
		device->onSharedHardwareDisconnected();
	}
}

void DMXKingFTDIManager::sendDMXData(const String& serial, int outputPort, const uint8* data)
{
	SharedFTDIHardware* hw = findHardware(serial);
	if (hw == nullptr || hw->handle == NULL)
		return;

	const ScopedLock sl(hw->lock);

	uint8 outputPortLabel = DMXKING_SEND_PORT1_LABEL + (outputPort - 1);

	uint8 frame[518];
	frame[0] = DMXKING_START_MESSAGE;
	frame[1] = outputPortLabel;
	frame[2] = (uint8)((DMXKING_CHANNEL_COUNT + 1) & 0xFF);
	frame[3] = (uint8)(((DMXKING_CHANNEL_COUNT + 1) >> 8) & 0xFF);
	frame[4] = DMXKING_START_CODE;
	memcpy(frame + 5, data, 512);
	frame[517] = DMXKING_END_MESSAGE;

	DWORD written = 0;
	if (FT_Write(hw->handle, frame, 518, &written) != FT_OK)
	{
		DBG("DMXKing FTDI: Write failed");
	}
}

void DMXKingFTDIManager::pollDeviceCapabilities(SharedFTDIHardware* hw)
{
	const ScopedLock sl(hw->lock);

	uint8 request[5] = { DMXKING_START_MESSAGE, DMXKING_DMX_PORT_COUNT_LABEL, 0, 0, DMXKING_END_MESSAGE };

	DWORD written = 0;
	if (FT_Write(hw->handle, request, 5, &written) != FT_OK)
	{
		DBG("DMXKing FTDI: Failed to send port count request");
	}
	else
	{
		LOG("DMXKing FTDI: Polling port count");
	}
}

void DMXKingFTDIManager::run()
{
	while (!threadShouldExit())
	{
		for (auto* hw : hardware)
		{
			if (hw->handle == NULL)
				continue;

			processReadData(hw);
		}

		Thread::sleep(10);
	}
}

void DMXKingFTDIManager::processReadData(SharedFTDIHardware* hw)
{
	const ScopedLock sl(hw->lock);

	DWORD bytesAvailable = 0;
	if (FT_GetQueueStatus(hw->handle, &bytesAvailable) != FT_OK || bytesAvailable == 0)
		return;

	uint8 buffer[1024];
	DWORD toRead = jmin((DWORD)sizeof(buffer), bytesAvailable);
	DWORD bytesRead = 0;

	if (FT_Read(hw->handle, buffer, toRead, &bytesRead) == FT_OK && bytesRead > 0)
	{
		hw->readBuffer.addArray((const uint8*)buffer, (int)bytesRead);

		int endIndex = 0;
		Array<uint8> packet = getDMXPacket(hw->readBuffer, endIndex);
		while (packet.size() > 0)
		{
			processDMXPacket(hw, packet);
			hw->readBuffer.removeRange(0, endIndex);
			packet = getDMXPacket(hw->readBuffer, endIndex);
		}
	}
}

Array<uint8> DMXKingFTDIManager::getDMXPacket(Array<uint8> bytes, int& endIndex)
{
	if (bytes.size() < DMXKING_HEADER_LENGTH + 1) return Array<uint8>();
	int numBytes = bytes.size();

	for (int i = 0; i < numBytes; ++i)
	{
		if (bytes[i] == DMXKING_START_MESSAGE)
		{
			if (i + 3 >= numBytes) break;
			int length = (int)bytes[i + 2] + ((int)bytes[i + 3] << 8);
			if (bytes.size() - i < DMXKING_HEADER_LENGTH + length) continue;
			endIndex = i + DMXKING_HEADER_LENGTH + length;

			if (endIndex < numBytes && bytes[endIndex] == DMXKING_END_MESSAGE)
			{
				endIndex++;
				return Array<uint8>(bytes.getRawDataPointer() + i, DMXKING_HEADER_LENGTH + length);
			}
		}
	}

	return Array<uint8>();
}

void DMXKingFTDIManager::processDMXPacket(SharedFTDIHardware* hw, Array<uint8> bytes)
{
	int messageLabel = (int)bytes[1];
	int length = (int)bytes[2] + ((int)bytes[3] << 8);

	switch (messageLabel)
	{
	case DMXKING_RECEIVE_LABEL:
	{
		if (length > DMXKING_CHANNEL_COUNT + 2)
		{
			LOGWARNING("DMXKing FTDI: bad DMX Length: " << length);
			return;
		}

		if (length > bytes.size())
		{
			LOGWARNING("DMXKing FTDI: bad data size: " << bytes.size() << " <> " << length);
			return;
		}

		if (bytes[DMXKING_HEADER_LENGTH] != DMXKING_START_CODE)
		{
			LOGWARNING("DMXKing FTDI: bad start code " << bytes[DMXKING_HEADER_LENGTH] << " should be " << DMXKING_START_CODE);
			return;
		}

		for (auto* device : hw->connectedDevices)
		{
			if (device->inputCC && device->inputCC->enabled->boolValue())
			{
				device->setDMXValuesIn(length - 1, bytes.getRawDataPointer() + DMXKING_HEADER_LENGTH + 1);
			}
		}
	}
	break;

	case DMXKING_RECEIVE_SERIAL_NUMBER_LABEL:
	{
		if (length >= 4)
		{
			int serialNumber = (int)(bytes[4] + (bytes[5] << 8) + (bytes[6] << 16) + (bytes[7] << 24));
			LOG("DMXKing FTDI: Got serial number: " << String::toHexString(serialNumber));
		}
	}
	break;

	case DMXKING_DMX_PORT_COUNT_LABEL:
	{
		if (length >= 1)
		{
			int portCount = (int)bytes[4];
			hw->detectedOutputPortCount = portCount;
			hw->outputPortCountDetected = true;
			notifyDevicesOutputPortCountChanged(hw);
		}
		else
		{
			LOGWARNING("DMXKing FTDI: Invalid port count response length: " << length);
		}
	}
	break;

	default:
		break;
	}
}

void DMXKingFTDIManager::notifyDevicesOutputPortCountChanged(SharedFTDIHardware* hw)
{
	LOG("DMXKing FTDI: Detected " + String(hw->detectedOutputPortCount) + " output ports");

	for (auto* device : hw->connectedDevices)
	{
		device->onPortCountDetected(hw->detectedOutputPortCount);
	}
}

#else

// Stub implementation for unsupported platforms
juce_ImplementSingleton(DMXKingFTDIManager);

DMXKingFTDIManager::DMXKingFTDIManager() {}
DMXKingFTDIManager::~DMXKingFTDIManager() {}

void DMXKingFTDIManager::registerDevice(DMXKingFTDIDevice*, const String&) {}
void DMXKingFTDIManager::unregisterDevice(DMXKingFTDIDevice*) {}
void DMXKingFTDIManager::sendDMXData(const String&, int, const uint8*) {}

#endif

