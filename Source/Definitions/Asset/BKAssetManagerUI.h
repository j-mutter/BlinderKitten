/*
  ==============================================================================

    BKAssetManagerUI.h
    Created: 8 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#pragma once
#include "JuceHeader.h"
#include "BKAsset.h"

class BKAssetManagerUI :
	public BaseManagerShapeShifterUI<BaseManager<BKAsset>, BKAsset, BaseItemUI<BKAsset>>
{
public:
	BKAssetManagerUI(const String &contentName);
	~BKAssetManagerUI();

	static BKAssetManagerUI * create(const String &name) { return new BKAssetManagerUI(name); }
};
