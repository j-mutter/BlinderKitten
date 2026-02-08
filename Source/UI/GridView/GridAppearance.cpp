/*
  ==============================================================================

    GridAppearance.cpp
    Created: 8 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#include "GridAppearance.h"

GridAppearance::GridAppearance() :
	ControllableContainer("Appearance")
{
	iconAssetId = addIntParameter("Icon Asset ID", "ID of the asset to use as icon (0 = no icon)", 0, 0);
	gridBackgroundColor = addColorParameter("Background Color", "Custom background color for grid buttons", Colour(40, 40, 40));
	gridBackgroundColor->canBeDisabledByUser = true;
	gridBackgroundColor->setEnabled(false);
}

GridAppearance::~GridAppearance()
{
}
