/*
  ==============================================================================

    DMXKingFTDIManager.h
    Created: 15 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#pragma once

class DMXKingFTDIDevice;

#if JUCE_MAC || JUCE_LINUX

#include <ftdi.h>

class DMXKingFTDIManager : public juce::Thread
{
public:
	juce_DeclareSingleton(DMXKingFTDIManager, true)
	DMXKingFTDIManager();
	~DMXKingFTDIManager();

	struct SharedFTDIHardware
	{
		struct ftdi_context ftdi;
		String serial;
		Array<DMXKingFTDIDevice*> connectedDevices;
		Array<uint8> readBuffer;
		int detectedOutputPortCount;
		bool outputPortCountDetected;
		CriticalSection lock;

		SharedFTDIHardware() : detectedOutputPortCount(1), outputPortCountDetected(false)
		{
			bzero(&ftdi, sizeof(struct ftdi_context));
			ftdi_init(&ftdi);
		}

		~SharedFTDIHardware()
		{
			ftdi_deinit(&ftdi);
		}
	};

	OwnedArray<SharedFTDIHardware> hardware;

	void registerDevice(DMXKingFTDIDevice* device, const String& serial);
	void unregisterDevice(DMXKingFTDIDevice* device);

	SharedFTDIHardware* findHardware(const String& serial);
	bool openHardware(SharedFTDIHardware* hw, const String& serial);
	void closeHardware(SharedFTDIHardware* hw);

	void sendDMXData(const String& serial, int outputPort, const uint8* data);
	void pollDeviceCapabilities(SharedFTDIHardware* hw);

	void run() override;

private:
	void processReadData(SharedFTDIHardware* hw);
	void processDMXPacket(SharedFTDIHardware* hw, Array<uint8> bytes);
	Array<uint8> getDMXPacket(Array<uint8> bytes, int& endIndex);
	void notifyDevicesOutputPortCountChanged(SharedFTDIHardware* hw);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DMXKingFTDIManager)
};

#elif JUCE_WINDOWS

#include <ftd2xx.h>

class DMXKingFTDIManager : public juce::Thread
{
public:
	juce_DeclareSingleton(DMXKingFTDIManager, true)
	DMXKingFTDIManager();
	~DMXKingFTDIManager();

	struct SharedFTDIHardware
	{
		FT_HANDLE handle;
		int deviceIndex;
		String serial;
		Array<DMXKingFTDIDevice*> connectedDevices;
		Array<uint8> readBuffer;
		int detectedOutputPortCount;
		bool outputPortCountDetected;
		CriticalSection lock;

		SharedFTDIHardware() : handle(NULL), deviceIndex(-1), detectedOutputPortCount(1), outputPortCountDetected(false) {}
	};

	OwnedArray<SharedFTDIHardware> hardware;

	void registerDevice(DMXKingFTDIDevice* device, const String& serial);
	void unregisterDevice(DMXKingFTDIDevice* device);

	SharedFTDIHardware* findHardware(const String& serial);
	bool openHardware(SharedFTDIHardware* hw, int deviceIndex);
	void closeHardware(SharedFTDIHardware* hw);

	void sendDMXData(const String& serial, int outputPort, const uint8* data);
	void pollDeviceCapabilities(SharedFTDIHardware* hw);

	void run() override;

private:
	void processReadData(SharedFTDIHardware* hw);
	void processDMXPacket(SharedFTDIHardware* hw, Array<uint8> bytes);
	Array<uint8> getDMXPacket(Array<uint8> bytes, int& endIndex);
	void notifyDevicesOutputPortCountChanged(SharedFTDIHardware* hw);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DMXKingFTDIManager)
};

#else

// Stub for unsupported platforms
class DMXKingFTDIManager
{
public:
	juce_DeclareSingleton(DMXKingFTDIManager, true)
	DMXKingFTDIManager();
	~DMXKingFTDIManager();

	void registerDevice(DMXKingFTDIDevice* device, const String& serial);
	void unregisterDevice(DMXKingFTDIDevice* device);
	void sendDMXData(const String& serial, int outputPort, const uint8* data);
};

#endif
