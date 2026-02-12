/*
  ==============================================================================

    GridAppearance.cpp
    Created: 8 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#include "GridAppearance.h"

GridAppearance::GridAppearance(Colour background, Colour highlight) :
	ControllableContainer("Appearance")
{
	defaultBackgoundColour = background;
	defaultHighlightColour = highlight;

	iconAssetId = addIntParameter("Icon Asset ID", "ID of the asset to use as icon (0 = no icon)", 0, 0);
	backgroundColourParam = addColorParameter("Background Color", "Custom background color for grid buttons", defaultBackgoundColour);
	textColourParam = addColorParameter("Text Color", "Custom color text when the button is off", Colour(188, 188, 188));
}

GridAppearance::~GridAppearance()
{
}

Colour GridAppearance::backgroundColour() {
	return backgroundColourParam->getColor();
}

Colour GridAppearance::highlightColour() {
	if (backgroundColour() == defaultBackgoundColour) {
		return defaultHighlightColour;
	}
	
	return backgroundColour().brighter(0.2);
}


Colour GridAppearance::textColour() {
	return textColourParam->getColor();
}

void GridAppearance::updateDefaultColours(Colour background, Colour highlight) {
	if (backgroundColour() == defaultBackgoundColour) {
		backgroundColourParam->setColor(background);
		defaultBackgoundColour = background;
		defaultHighlightColour = highlight;
	}
}
