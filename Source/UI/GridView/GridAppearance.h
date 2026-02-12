/*
  ==============================================================================

    GridAppearance.h
    Created: 8 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#pragma once
#include "JuceHeader.h"

class GridAppearance : public ControllableContainer
{
public:
	GridAppearance(Colour background = Colour(40, 40, 40), Colour highlight = Colour(165, 85, 27));
	~GridAppearance();

	IntParameter* iconAssetId;

	ColorParameter* backgroundColourParam;
	ColorParameter* textColourParam;

	Colour backgroundColour();
	Colour highlightColour();

	Colour textColour();

	void updateDefaultColours(Colour background, Colour highlight);
private:
	Colour defaultBackgoundColour;
	Colour defaultHighlightColour;
};
