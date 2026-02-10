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

    setColour(TextButton::buttonColourId, a->backgroundColor->getColor());
    setColour(TextButton::textColourOffId, a->textColor->getColor());
    setColour(TextButton::buttonOnColourId, a->highlightColor->getColor());
    setColour(TextButton::textColourOnId, a->highlightTextColor->getColor());
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
    setColour(TextButton::buttonColourId, Colour(32, 32, 32));
    removeColour(TextButton::buttonOnColourId);
    removeColour(TextButton::textColourOnId);
    removeColour(TextButton::textColourOffId);
}

void VirtualButtonGridButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    // Determine background colour based on status
    Colour bgCol = findColour(TextButton::buttonColourId);

    if (currentStatus == BTN_GENERIC) {
        bgCol = juce::Colour(64, 64, 80);
    }
    else if (currentStatus == BTN_ON) {
        bgCol = findColour(TextButton::buttonOnColourId);
    }
    else if (currentStatus == BTN_ON_LOADED) {
        bgCol = juce::Colour(64, 80, 80);
    }
    else if (currentStatus == BTN_OFF_LOADED) {
        bgCol = juce::Colour(64, 80, 80);
    }
    else if (currentStatus == BTN_CURRENTCUE) {
        bgCol = juce::Colour(64, 120, 64);
    }
    else if (currentStatus == BTN_LOADEDCUE) {
        bgCol = juce::Colour(64, 64, 120);
    }

    // Border: orange when active, dark otherwise
    bool isActive = (currentStatus == BTN_ON || currentStatus == BTN_ON_LOADED);
    Colour strokeCol = isActive ? Colour(252, 126, 36) : Colour(29, 29, 29);
    g.fillAll(strokeCol);

    g.setColour(bgCol);
    g.fillRect(getLocalBounds().reduced(1));

    // Choose text colour based on active state
    Colour textCol = isActive
        ? findColour(TextButton::textColourOnId)
        : findColour(TextButton::textColourOffId);

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
