/*
  ==============================================================================

    DMXKingFTDIDevice.h
    Created: 15 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#pragma once

class DMXKingFTDIManager;

#if JUCE_MAC || JUCE_LINUX

class DMXKingFTDIDevice : public DMXDevice
{
public:
	DMXKingFTDIDevice();
	~DMXKingFTDIDevice();

	EnumParameter* deviceSelector;
	EnumParameter* outputPort;

	void sendDMXValuesInternal() override;
	void onContainerParameterChanged(Parameter* p) override;

	void refreshDeviceList();
	int getCurrentOutputPortNumber() const;

	void onSharedHardwareConnected();
	void onSharedHardwareDisconnected();
	void onPortCountDetected(int portCount);

private:
	String selectedSerial;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DMXKingFTDIDevice)
};

#elif JUCE_WINDOWS

class DMXKingFTDIDevice : public DMXDevice
{
public:
	DMXKingFTDIDevice();
	~DMXKingFTDIDevice();

	EnumParameter* deviceSelector;
	EnumParameter* outputPort;

	void sendDMXValuesInternal() override;
	void onContainerParameterChanged(Parameter* p) override;

	void refreshDeviceList();
	int getCurrentOutputPortNumber() const;

	void onSharedHardwareConnected();
	void onSharedHardwareDisconnected();
	void onPortCountDetected(int portCount);

private:
	String selectedSerial;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DMXKingFTDIDevice)
};

#else

// Stub for unsupported platforms
class DMXKingFTDIDevice : public DMXDevice
{
public:
	DMXKingFTDIDevice();
	~DMXKingFTDIDevice();
	void sendDMXValuesInternal() override;
};

#endif
