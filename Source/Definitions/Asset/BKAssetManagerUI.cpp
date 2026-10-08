/*
  ==============================================================================

    BKAssetManagerUI.cpp
    Created: 8 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#include "BKAssetManagerUI.h"
#include "BKAssetManager.h"

BKAssetManagerUI::BKAssetManagerUI(const String & contentName) :
	BaseManagerShapeShifterUI(contentName, BKAssetManager::getInstance())
{
	addItemText = "Add new Asset";
	noItemText = "No assets yet. Add image assets to use as icons.";
	addExistingItems();
}

BKAssetManagerUI::~BKAssetManagerUI()
{
}
