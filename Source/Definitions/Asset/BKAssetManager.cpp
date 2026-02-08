/*
  ==============================================================================

    BKAssetManager.cpp
    Created: 8 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#include "BKAsset.h"
#include "BKAssetManager.h"

juce_ImplementSingleton(BKAssetManager);

int compareAsset(BKAsset* A, BKAsset* B) {
    return (int)A->id->getValue() - (int)B->id->getValue();
}

BKAssetManager::BKAssetManager() :
    BaseManager("Asset")
    {
    itemDataType = "Asset";
    selectItemWhenCreated = true;
    comparator.compareFunc = compareAsset;
    autoReorderOnAdd = true;
}

BKAssetManager::~BKAssetManager()
{
}

void BKAssetManager::addItemInternal(BKAsset* o, var data)
{
}

void BKAssetManager::removeItemInternal(BKAsset* o)
{
}

void BKAssetManager::onContainerParameterChanged(Parameter* p)
{
}
