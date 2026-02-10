/*
  ==============================================================================

    VirtualButtonGridButton.h
    Created: 9 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class GridAppearance;
class VirtualButton;

class VirtualButtonGridButton :
    public TextButton
{
public:
    VirtualButtonGridButton();
    ~VirtualButtonGridButton();

    enum ButtonStatus { BTN_UNASSIGNED, BTN_ON, BTN_OFF, BTN_ON_LOADED, BTN_OFF_LOADED, BTN_GENERIC, BTN_CURRENTCUE, BTN_LOADEDCUE };
	static HashMap<ButtonStatus, Colour> defaultColours;

    void updateFromAppearance(GridAppearance* a, String name);
    void updateStatus(ButtonStatus status);
    void clear();

    Image iconImage;

    void paintButton(juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    ButtonStatus currentStatus = BTN_UNASSIGNED;
};
