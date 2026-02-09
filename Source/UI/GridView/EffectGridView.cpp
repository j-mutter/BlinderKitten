/*
  ==============================================================================

    EffectGridView.cpp
    Created: 19 Feb 2022 12:19:42am
    Author:  No

  ==============================================================================
*/

#include <JuceHeader.h>
#include "EffectGridView.h"
#include "Brain.h"
#include "Definitions/Effect/Effect.h"
#include "Definitions/Effect/EffectManager.h"
#include "DataTransferManager/DataTransferManager.h"

//==============================================================================
EffectGridViewUI::EffectGridViewUI(const String& contentName):
    ShapeShifterContent(EffectGridView::getInstance(), contentName)
{
    
}

EffectGridViewUI::~EffectGridViewUI()
{
}

juce_ImplementSingleton(EffectGridView);

EffectGridView::EffectGridView()
{
    numberOfCells = 200;
    targetType = "Effect";
    EffectManager::getInstance()->addAsyncManagerListener(this);

}

EffectGridView::~EffectGridView()
{
    if (EffectManager::getInstanceWithoutCreating() != nullptr) EffectManager::getInstance()->removeAsyncManagerListener(this);
}

void EffectGridView::updateCells() {
    for (int i = 0; i < numberOfCells; i++) {
        Effect* g = Brain::getInstance()->getEffectById(i+1);
		if (g != nullptr) {
			gridButtons[i]->updateFromAppearance(&g->gridAppearance, g->userName->getValue());
			gridButtons[i]->setToggleState(g->isEffectOn->boolValue(), NotificationType::dontSendNotification);
		} else {
			gridButtons[i]->clear();
		}
		gridButtons[i]->repaint();
    }
}

void EffectGridView::showContextMenu(int id)
{
    Effect* target = Brain::getInstance()->getEffectById(id);
    PopupMenu p;
    if (target != nullptr) {
        p.addItem("Start", [target]() {target->start(); });
        p.addItem("Stop", [target]() {target->stop(); });
    }
    else {
        p.addItem("Add", [id]() {DataTransferManager::getInstance()->editObject("effect", id); });
    }
    p.showMenuAsync(PopupMenu::Options(), [this](int result) {});
}

void EffectGridView::newMessage(const EffectManager::ManagerEvent& e)
{
    updateCells();
}
