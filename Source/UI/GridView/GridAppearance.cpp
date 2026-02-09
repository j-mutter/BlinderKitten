/*
  ==============================================================================

    GridAppearance.cpp
    Created: 8 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#include "GridAppearance.h"

GridAppearance::GridAppearance(bool canHighlight) :
	ControllableContainer("Appearance")
{
	iconAssetId = addIntParameter("Icon Asset ID", "ID of the asset to use as icon (0 = no icon)", 0, 0);
	backgroundColor = addColorParameter("Background Color", "Custom background color for grid buttons", Colour(40, 40, 40));
	textColor = addColorParameter("Text Color", "Custom color text when the button is off", Colour(188, 188, 188));
	
	highlightColor = addColorParameter("Highlight Color", "Custom highlight color for grid buttons", Colour(64, 80, 64));
	highlightTextColor = addColorParameter("Text Highlight Color", "Custom color text when the button is off", Colour(255, 255, 255));
	
	highlightColor->hideInEditor = !canHighlight;
	highlightTextColor->hideInEditor = !canHighlight;
}

GridAppearance::~GridAppearance()
{
}
