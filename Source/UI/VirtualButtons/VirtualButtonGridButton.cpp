/*
  ==============================================================================

    VirtualButtonGridButton.cpp
    Created: 9 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#include <JuceHeader.h>
#include "VirtualButtonGridButton.h"
#include "UI/GridView/GridAppearance.h"
#include "Brain.h"

VirtualButtonGridButton::VirtualButtonGridButton()
{
    setWantsKeyboardFocus(false);
    clear();
}

VirtualButtonGridButton::~VirtualButtonGridButton()
{
}

void VirtualButtonGridButton::updateFromAppearance(GridAppearance* a, String name)
{
    int assetId = a->iconAssetId->intValue();
    BKAsset* asset = (assetId > 0) ? Brain::getInstance()->getAssetById(assetId) : nullptr;

    if (asset != nullptr && asset->hasValidImage()) {
        iconImage = asset->getImage();
        setButtonText("");
    } else {
        iconImage = Image();
        setButtonText(name);
    }

    setColour(TextButton::buttonColourId, a->backgroundColour());
    setColour(TextButton::buttonOnColourId, a->highlightColour());
    setColour(TextButton::textColourOffId, a->textColour());
}

void VirtualButtonGridButton::updateStatus(ButtonStatus status)
{
    currentStatus = status;
}

void VirtualButtonGridButton::clear()
{
    setButtonText("");
    iconImage = Image();
    currentStatus = BTN_UNASSIGNED;
    setColour(TextButton::buttonColourId, Colour(40, 40, 40));
    removeColour(TextButton::buttonOnColourId);
    removeColour(TextButton::textColourOnId);
    removeColour(TextButton::textColourOffId);
}

void VirtualButtonGridButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    Colour bgCol = findColour(TextButton::buttonColourId);
	
	g.fillAll(Colour(29, 29, 29)); // Dark grey for grid lines

	Colour strokeCol = bgCol;
	int strokeSize = 2;
    if (currentStatus == BTN_GENERIC) {
		strokeCol = juce::Colour(100, 10, 100); // Purple
    }
    else if (currentStatus == BTN_ON) {
		strokeCol = Colour(252, 126, 36); // Orange like other grid buttons when active
    }
    else if (currentStatus == BTN_ON_LOADED) {
		strokeCol = Colour(30, 165, 234); // Bright Blue
    }
    else if (currentStatus == BTN_OFF_LOADED) {
		strokeCol = juce::Colour(28, 142, 113); // Dull blue
    }
    else if (currentStatus == BTN_CURRENTCUE) {
		strokeCol =  juce::Colour(93, 171, 57); // Bright Green
    }
    else if (currentStatus == BTN_LOADEDCUE) {
		strokeCol = juce::Colour(64, 120, 64); // Dull green
	} else {
		// No stroke for OFF or UNASSIGNED
		strokeSize = 0;
	}

	// Reduce to show 1px grid lines on all sides
	auto bounds = getLocalBounds().reduced(1);

	if (strokeSize > 0) {
		g.setColour(strokeCol);
		g.fillRect(bounds);
		bounds = bounds.reduced(strokeSize);
	}

	g.setColour(bgCol);
	g.fillRect(bounds);

    Colour textCol = findColour(TextButton::textColourOnId);

    if (iconImage.isValid()) {
        auto bounds = getLocalBounds().reduced(4);
        float imgAspect = (float)iconImage.getWidth() / iconImage.getHeight();
        float boundsAspect = (float)bounds.getWidth() / bounds.getHeight();
        int drawW, drawH;
        if (imgAspect > boundsAspect) {
            drawW = bounds.getWidth();
            drawH = (int)(drawW / imgAspect);
        } else {
            drawH = bounds.getHeight();
            drawW = (int)(drawH * imgAspect);
        }
        int drawX = bounds.getX() + (bounds.getWidth() - drawW) / 2;
        int drawY = bounds.getY() + (bounds.getHeight() - drawH) / 2;
        g.drawImage(iconImage, drawX, drawY, drawW, drawH, 0, 0, iconImage.getWidth(), iconImage.getHeight());
    } else {
        g.setColour(textCol);
        g.drawFittedText(getButtonText(), getLocalBounds().reduced(2), Justification::centred, 2);
    }

    if (shouldDrawButtonAsHighlighted) {
        g.fillAll(Colours::white.withAlpha(0.15f));
    } else if (shouldDrawButtonAsDown) {
        g.fillAll(Colours::white.withAlpha(0.01f));
    }
}
