/*
  ==============================================================================

    PresetGridView.cpp
    Created: 19 Feb 2022 12:19:42am
    Author:  No

  ==============================================================================
*/

#include <JuceHeader.h>
#include "PresetGridView.h"
#include "Brain.h"
#include "Definitions/Preset/Preset.h"
#include "Definitions/Preset/PresetManager.h"
#include "Definitions/Asset/BKAsset.h"
#include "DataTransferManager/DataTransferManager.h"

//==============================================================================
PresetGridViewUI::PresetGridViewUI(const String& contentName):
    ShapeShifterContent(PresetGridView::getInstance(), contentName)
{
    
}

PresetGridViewUI::~PresetGridViewUI()
{
}

juce_ImplementSingleton(PresetGridView);

PresetGridView::PresetGridView()
{
    numberOfCells = 200;
    targetType = "preset";
    PresetManager::getInstance()->addAsyncManagerListener(this);

}

PresetGridView::~PresetGridView()
{
    if (PresetManager::getInstanceWithoutCreating() != nullptr) PresetManager::getInstance()->removeAsyncManagerListener(this);
}

void PresetGridView::updateCells() {
    for (int i = 0; i < numberOfCells; i++) {
        Preset* g = Brain::getInstance()->getPresetById(i+1);
        if (g != nullptr) {
			gridButtons[i]->updateFromAppearance(&g->gridAppearance, g->userName->getValue());
        }
        else {
			gridButtons[i]->clear();
        }
        gridButtons[i]->repaint();
    }
}

void PresetGridView::newMessage(const PresetManager::ManagerEvent& e)
{
    updateCells();
}
