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
	GridAppearance(bool canHighlight = true);
	~GridAppearance();

	IntParameter* iconAssetId;
	
	ColorParameter* backgroundColor;
	ColorParameter* textColor;
	
	ColorParameter* highlightColor;
	ColorParameter* highlightTextColor;
};
