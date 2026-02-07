/*
  ==============================================================================

    DMXOpenUSBFTDIDevice.h
    Created: 7 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#pragma once

#if JUCE_MAC || JUCE_LINUX

#include <ftdi.h>

struct FTDIDeviceInfo
{
	String serial;
	String name;
	String vendor;
	uint16 vid;
	uint16 pid;
};

class DMXOpenUSBFTDIDevice : public DMXDevice
{
public:
	DMXOpenUSBFTDIDevice();
	~DMXOpenUSBFTDIDevice();

	static const uint16 FTDI_VID = 0x0403;
	static const uint16 FTDI_PID = 0x6001;
	static const uint16 FTDI2_PID = 0x6010;

	EnumParameter* deviceSelector;

	void sendDMXValuesInternal() override;
	void onContainerParameterChanged(Parameter* p) override;

	void refreshDeviceList();
	bool openDevice(const String& serial, const String& name);
	void closeDevice();

	static Array<FTDIDeviceInfo> enumerateDevices();

private:
	struct ftdi_context ftdi;
	bool isOpen;
	String selectedSerial;
	unsigned char defaultLatency;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DMXOpenUSBFTDIDevice)
};

#else

// Stub for platforms without libftdi support
class DMXOpenUSBFTDIDevice : public DMXDevice
{
public:
	DMXOpenUSBFTDIDevice();
	~DMXOpenUSBFTDIDevice();
	void sendDMXValuesInternal() override;
};

#endif
