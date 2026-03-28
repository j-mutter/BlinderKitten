/*
  ==============================================================================

    DMXUSBDevice.h
    Created: 16 Feb 2026
    Author:  j-mutter

    Unified USB DMX device: auto-detects protocol from device name/VID/PID.
    Supports Open DMX, Enttec Pro, DMXKing, and Eurolite protocols
    over both FTDI and serial backends.

  ==============================================================================
*/

#pragma once

enum USBDMXMode
{
	USBDMX_AUTO = 0,
	USBDMX_OPEN_DMX,
	USBDMX_ENTTEC_PRO,
	USBDMX_DMXKING,
	USBDMX_EUROLITE
};

struct DetectedUSBDevice
{
	String name;
	String serial;
	String portPath;      // serial port path (empty for FTDI-only)
	int ftdiIndex;        // FTDI device index for Windows (-1 if not FTDI)
	uint16 vid;
	uint16 pid;
	bool isFTDI;
	USBDMXMode detectedMode;

	String getDisplayLabel() const
	{
		String label = name.isNotEmpty() ? name : serial;
		if (serial.isNotEmpty() && name.isNotEmpty())
			label += " (" + serial + ")";
		return label;
	}

	String getUniqueID() const
	{
		if (isFTDI && serial.isNotEmpty())
			return "ftdi:" + serial;
		if (portPath.isNotEmpty())
			return "serial:" + portPath;
		return "ftdi_idx:" + String(ftdiIndex);
	}
};

#if JUCE_MAC || JUCE_LINUX

#include <ftdi.h>

class DMXUSBDevice :
	public DMXDevice,
	public SerialManager::SerialManagerListener,
	public SerialDevice::SerialDeviceListener,
	public Timer
{
public:
	DMXUSBDevice();
	~DMXUSBDevice();

	EnumParameter* deviceSelector;
	EnumParameter* modeSelector;
	EnumParameter* outputPortSelector;

	void sendDMXValuesInternal() override;
	void onContainerParameterChanged(Parameter* p) override;
	void timerCallback() override;

	void refreshDeviceList();
	static USBDMXMode autoDetectMode(const String& name, uint16 vid, uint16 pid);
	static String modeToString(USBDMXMode mode);

	// SerialManager listener
	void portAdded(SerialDeviceInfo* info) override;
	void portRemoved(SerialDeviceInfo* info) override;

	// SerialDevice listener
	void portOpened(SerialDevice*) override;
	void portClosed(SerialDevice*) override;
	void portRemoved(SerialDevice*) override;
	void serialDataReceived(const var& data) override;

private:
	Array<DetectedUSBDevice> detectedDevices;
	DetectedUSBDevice currentDevice;
	USBDMXMode activeMode;
	int dmxKingPortCount;

	// FTDI connection state
	struct ftdi_context ftdi;
	bool ftdiIsOpen;
	unsigned char ftdiDefaultLatency;

	// Serial connection state
	SerialDevice* serialPort;
	String lastOpenedPortID;

	// Enttec Pro input buffer
	Array<uint8> serialBuffer;

	// FTDI hot-swap tracking
	StringArray lastFTDISerials;

	void openConnection();
	void closeConnection();

	// FTDI operations
	bool openFTDI(const String& serial, const String& name);
	void closeFTDI();
	void sendFTDI_OpenDMX();
	void sendFTDI_EnttecPro();
	void sendFTDI_DMXKing();
	void sendFTDI_Eurolite();

	// Serial operations
	bool openSerial(const String& portPath);
	void closeSerial();
	void configureSerialPort_OpenDMX();
	void configureSerialPort_EnttecPro();
	void configureSerialPort_Eurolite();
	void sendSerial_OpenDMX();
	void sendSerial_EnttecPro();
	void sendSerial_DMXKing();
	void sendSerial_Eurolite();

	// DMXKing detection
	bool probeDMXKingFTDI();
	void updateDeviceLabel(const String& uniqueID, USBDMXMode newMode);
	void updateOutputPortVisibility();

	// Enttec Pro input parsing
	void parseEnttecProInput(const uint8* data, int numBytes);
	Array<uint8> getEnttecProPacket(Array<uint8>& buffer, int& endIndex);
	void processEnttecProPacket(const Array<uint8>& packet);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DMXUSBDevice)
};

#elif JUCE_WINDOWS

#include <ftd2xx.h>

class DMXUSBDevice :
	public DMXDevice,
	public SerialManager::SerialManagerListener,
	public SerialDevice::SerialDeviceListener,
	public Timer
{
public:
	DMXUSBDevice();
	~DMXUSBDevice();

	EnumParameter* deviceSelector;
	EnumParameter* modeSelector;
	EnumParameter* outputPortSelector;

	void sendDMXValuesInternal() override;
	void onContainerParameterChanged(Parameter* p) override;
	void timerCallback() override;

	void refreshDeviceList();
	static USBDMXMode autoDetectMode(const String& name, uint16 vid, uint16 pid);
	static String modeToString(USBDMXMode mode);

	// SerialManager listener
	void portAdded(SerialDeviceInfo* info) override;
	void portRemoved(SerialDeviceInfo* info) override;

	// SerialDevice listener
	void portOpened(SerialDevice*) override;
	void portClosed(SerialDevice*) override;
	void portRemoved(SerialDevice*) override;
	void serialDataReceived(const var& data) override;

private:
	Array<DetectedUSBDevice> detectedDevices;
	DetectedUSBDevice currentDevice;
	USBDMXMode activeMode;
	int dmxKingPortCount;

	// FTDI connection state
	FT_HANDLE ftdiHandle;
	bool ftdiIsOpen;
	unsigned char ftdiDefaultLatency;

	// Serial connection state
	SerialDevice* serialPort;
	String lastOpenedPortID;

	// Enttec Pro input buffer
	Array<uint8> serialBuffer;

	// FTDI hot-swap tracking
	StringArray lastFTDISerials;

	void openConnection();
	void closeConnection();

	// FTDI operations
	bool openFTDI(int deviceIndex);
	void closeFTDI();
	void sendFTDI_OpenDMX();
	void sendFTDI_EnttecPro();
	void sendFTDI_DMXKing();
	void sendFTDI_Eurolite();

	// Serial operations
	bool openSerial(const String& portPath);
	void closeSerial();
	void configureSerialPort_OpenDMX();
	void configureSerialPort_EnttecPro();
	void configureSerialPort_Eurolite();
	void sendSerial_OpenDMX();
	void sendSerial_EnttecPro();
	void sendSerial_DMXKing();
	void sendSerial_Eurolite();

	// DMXKing detection
	bool probeDMXKingFTDI();
	void updateDeviceLabel(const String& uniqueID, USBDMXMode newMode);
	void updateOutputPortVisibility();

	// Enttec Pro input parsing
	void parseEnttecProInput(const uint8* data, int numBytes);
	Array<uint8> getEnttecProPacket(Array<uint8>& buffer, int& endIndex);
	void processEnttecProPacket(const Array<uint8>& packet);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DMXUSBDevice)
};

#else

// Stub for unsupported platforms
class DMXUSBDevice : public DMXDevice
{
public:
	DMXUSBDevice();
	~DMXUSBDevice();
	void sendDMXValuesInternal() override;
};

#endif
