/*
  ==============================================================================

    DMXKingFTDIDevice.cpp
    Created: 15 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#include "DMXKingFTDIDevice.h"
#include "DMXKingFTDIManager.h"

#if JUCE_MAC || JUCE_LINUX

DMXKingFTDIDevice::DMXKingFTDIDevice() :
	DMXDevice("DMXKing FTDI", DMXKING_FTDI, true)
{
	inputCC->enabled->setValue(false);

	deviceSelector = addEnumParameter("FTDI Device", "Select the FTDI USB device to use");
	deviceSelector->addOption("None", "none");
	refreshDeviceList();

	outputPort = outputCC->addEnumParameter("Output Port", "Select which physical output port to use");
	outputPort->addOption("Port A", 1);
	outputPort->setValueWithKey("Port A");
}

DMXKingFTDIDevice::~DMXKingFTDIDevice()
{
	if (DMXKingFTDIManager::getInstanceWithoutCreating())
	{
		DMXKingFTDIManager::getInstance()->unregisterDevice(this);
	}
}

void DMXKingFTDIDevice::refreshDeviceList()
{
	String previousSelection = selectedSerial;
	deviceSelector->clearOptions();
	deviceSelector->addOption("None", "none");

	Array<FTDIDeviceInfo> devices = DMXOpenUSBFTDIDevice::enumerateDevices();

	for (const auto& dev : devices)
	{
		String label = dev.name.isEmpty() ? dev.serial : dev.name + " (" + dev.serial + ")";
		deviceSelector->addOption(label, dev.serial);
	}

	if (previousSelection.isNotEmpty() && previousSelection != "none")
	{
		deviceSelector->setValueWithData(previousSelection);
	}
}

int DMXKingFTDIDevice::getCurrentOutputPortNumber() const
{
	return outputPort->getValueData();
}

void DMXKingFTDIDevice::sendDMXValuesInternal()
{
	if (selectedSerial.isEmpty() || selectedSerial == "none")
		return;

	int portNumber = getCurrentOutputPortNumber();
	if (portNumber < 1 || portNumber > 8)
		return;

	DMXKingFTDIManager::getInstance()->sendDMXData(selectedSerial, portNumber, dmxDataOut);
}

void DMXKingFTDIDevice::onContainerParameterChanged(Parameter* p)
{
	DMXDevice::onContainerParameterChanged(p);

	if (p == deviceSelector)
	{
		String newSerial = deviceSelector->getValueData().toString();

		// Unregister from previous hardware
		if (selectedSerial.isNotEmpty() && selectedSerial != "none")
		{
			DMXKingFTDIManager::getInstance()->unregisterDevice(this);
			setConnected(false);
		}

		selectedSerial = newSerial;

		if (newSerial.isNotEmpty() && newSerial != "none")
		{
			DMXKingFTDIManager::getInstance()->registerDevice(this, newSerial);
			setConnected(true);
		}
	}
}

void DMXKingFTDIDevice::onSharedHardwareConnected()
{
	setConnected(true);
}

void DMXKingFTDIDevice::onSharedHardwareDisconnected()
{
	setConnected(false);
}

void DMXKingFTDIDevice::onPortCountDetected(int portCount)
{
	outputPort->clearOptions();

	for (int i = 1; i <= portCount; ++i)
	{
		char portLetter = 'A' + (i - 1);
		outputPort->addOption("Port " + String::charToString(portLetter), i);
	}

	int currentPort = getCurrentOutputPortNumber();
	if (currentPort > portCount)
	{
		outputPort->setValueWithKey("Port A");
	}
}

#elif JUCE_WINDOWS

DMXKingFTDIDevice::DMXKingFTDIDevice() :
	DMXDevice("DMXKing FTDI", DMXKING_FTDI, true)
{
	inputCC->enabled->setValue(false);

	deviceSelector = addEnumParameter("FTDI Device", "Select the FTDI USB device to use");
	deviceSelector->addOption("None", -1);
	refreshDeviceList();

	outputPort = outputCC->addEnumParameter("Output Port", "Select which physical output port to use");
	outputPort->addOption("Port A", 1);
	outputPort->setValueWithKey("Port A");
}

DMXKingFTDIDevice::~DMXKingFTDIDevice()
{
	if (DMXKingFTDIManager::getInstanceWithoutCreating())
	{
		DMXKingFTDIManager::getInstance()->unregisterDevice(this);
	}
}

void DMXKingFTDIDevice::refreshDeviceList()
{
	deviceSelector->clearOptions();
	deviceSelector->addOption("None", -1);

	Array<FTDIDeviceInfo> devices = DMXOpenUSBFTDIDevice::enumerateDevices();

	for (const auto& dev : devices)
	{
		String label = dev.name.isEmpty() ? dev.serial : dev.name + " (" + dev.serial + ")";
		deviceSelector->addOption(label, (int)dev.id);
	}
}

int DMXKingFTDIDevice::getCurrentOutputPortNumber() const
{
	return outputPort->getValueData();
}

void DMXKingFTDIDevice::sendDMXValuesInternal()
{
	if (selectedSerial.isEmpty())
		return;

	int portNumber = getCurrentOutputPortNumber();
	if (portNumber < 1 || portNumber > 8)
		return;

	DMXKingFTDIManager::getInstance()->sendDMXData(selectedSerial, portNumber, dmxDataOut);
}

void DMXKingFTDIDevice::onContainerParameterChanged(Parameter* p)
{
	DMXDevice::onContainerParameterChanged(p);

	if (p == deviceSelector)
	{
		int selectedIndex = (int)deviceSelector->getValueData();

		// Unregister from previous hardware
		if (selectedSerial.isNotEmpty())
		{
			DMXKingFTDIManager::getInstance()->unregisterDevice(this);
			setConnected(false);
		}

		if (selectedIndex < 0)
		{
			selectedSerial = "";
		}
		else
		{
			selectedSerial = String(selectedIndex);
			DMXKingFTDIManager::getInstance()->registerDevice(this, selectedSerial);
			setConnected(true);
		}
	}
}

void DMXKingFTDIDevice::onSharedHardwareConnected()
{
	setConnected(true);
}

void DMXKingFTDIDevice::onSharedHardwareDisconnected()
{
	setConnected(false);
}

void DMXKingFTDIDevice::onPortCountDetected(int portCount)
{
	outputPort->clearOptions();

	for (int i = 1; i <= portCount; ++i)
	{
		char portLetter = 'A' + (i - 1);
		outputPort->addOption("Port " + String::charToString(portLetter), i);
	}

	int currentPort = getCurrentOutputPortNumber();
	if (currentPort > portCount)
	{
		outputPort->setValueWithKey("Port A");
	}
}

#else

// Stub implementation for unsupported platforms
DMXKingFTDIDevice::DMXKingFTDIDevice() :
	DMXDevice("DMXKing FTDI", DMXKING_FTDI, false)
{
	LOGWARNING("DMXKing FTDI is not supported on this platform");
}

DMXKingFTDIDevice::~DMXKingFTDIDevice()
{
}

void DMXKingFTDIDevice::sendDMXValuesInternal()
{
}

#endif
