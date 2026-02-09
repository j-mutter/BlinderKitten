/*
  ==============================================================================

    BKButton.cpp
    Created: 9 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#include <JuceHeader.h>
#include "GridView.h"
#include "GridViewButton.h"
#include "GridAppearance.h"
#include "Brain.h"

GridViewButton::GridViewButton()
{
    setWantsKeyboardFocus(false);
    clear();
}

GridViewButton::~GridViewButton()
{
}

void GridViewButton::updateFromAppearance(GridAppearance* a, String name)
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

void GridViewButton::clear()
{
    setButtonText("");
    iconImage = Image();
    setColour(TextButton::buttonColourId, Colour(40, 40, 40));
    removeColour(TextButton::buttonOnColourId);
    removeColour(TextButton::textColourOnId);
    removeColour(TextButton::textColourOffId);
}

void GridViewButton::paint(juce::Graphics& g)
{
    Colour strokeCol = getToggleState() ? Colour(252, 126, 36) : Colour(29, 29, 29);
    g.fillAll(strokeCol);

    Colour bgCol = findColour(getToggleState() ? TextButton::buttonOnColourId : TextButton::buttonColourId);
    g.setColour(bgCol);
    g.fillRect(getLocalBounds().reduced(1));

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

        if (id > 0) {
            g.setColour(Colours::white.withAlpha(0.7f));
            g.drawFittedText(String(id), getLocalBounds().reduced(2), Justification::topRight, 1);
        }
    } else {
        Colour textCol = findColour(getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId);
        g.setColour(textCol);
        g.drawFittedText(getButtonText(), getLocalBounds().reduced(2), Justification::centred, 2);
        if (id > 0) {
            g.setColour(Colours::white.withAlpha(0.7f));
            g.drawFittedText(String(id), getLocalBounds().reduced(2), Justification::topRight, 1);
        }
    }

    if (isMouseOver()) {
        g.fillAll(Colours::white.withAlpha(0.15f));
    } else if (isMouseButtonDown()) {
        g.fillAll(Colours::white.withAlpha(0.01f));
    }
}

void GridViewButton::mouseDown(const MouseEvent& e)
{
	if (e.mods.isRightButtonDown()) {
		return;
	}
	Button::mouseDown(e);
}

void GridViewButton::mouseUp(const MouseEvent& e)
{
	if (e.mods.isRightButtonDown()) {
		GridView* p = findParentComponentOfClass<GridView>();
		if (p != nullptr) {
			p->showContextMenu(id);
		}
		return;
	}
	Button::mouseUp(e);
}

void GridViewButton::mouseDrag(const MouseEvent& e)
{
	GridView* parent = findParentComponentOfClass<GridView>();
	if (parent != nullptr && e.getDistanceFromDragStart() > 10 && !isDragAndDropActive())
	{
		var dragData(new DynamicObject());
		dragData.getDynamicObject()->setProperty("type", "GridViewButton");
		dragData.getDynamicObject()->setProperty("targetType", parent->targetType);
		dragData.getDynamicObject()->setProperty("id", id);

		startDragging(dragData, this, ScaledImage(), true);
	}
}

