/*
  ==============================================================================

    BKAssetManager.h
    Created: 8 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#pragma once
#include "BKAsset.h"

class BKAssetManager :
    public BaseManager<BKAsset>
{
public:
    juce_DeclareSingleton(BKAssetManager, true);

    BKAssetManager();
    ~BKAssetManager();

    void addItemInternal(BKAsset* o, var data) override;
    void removeItemInternal(BKAsset* o) override;

    void onContainerParameterChanged(Parameter* p) override;

};
