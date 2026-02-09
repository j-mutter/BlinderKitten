/*
  ==============================================================================

    CuelistGridView.cpp
    Created: 19 Feb 2022 12:19:42am
    Author:  No

  ==============================================================================
*/

#include <JuceHeader.h>
#include "CuelistGridView.h"
#include "Brain.h"
#include "Definitions/Cuelist/Cuelist.h"
#include "Definitions/Cuelist/CuelistManager.h"
#include "UserInputManager.h"
#include "DataTransferManager/DataTransferManager.h"

//==============================================================================
CuelistGridViewUI::CuelistGridViewUI(const String& contentName):
    ShapeShifterContent(CuelistGridView::getInstance(), contentName)
{
    
}

CuelistGridViewUI::~CuelistGridViewUI()
{
}

juce_ImplementSingleton(CuelistGridView);

CuelistGridView::CuelistGridView()
{
    numberOfCells = 200;
    targetType = "Cuelist";
    CuelistManager::getInstance()->addAsyncManagerListener(this);

}

CuelistGridView::~CuelistGridView()
{
    if (CuelistManager::getInstanceWithoutCreating() != nullptr) CuelistManager::getInstance()->removeAsyncManagerListener(this);
}

void CuelistGridView::updateCells() {
    for (int i = 0; i < numberOfCells; i++) {
        Cuelist* g = Brain::getInstance()->getCuelistById(i+1);
		if (g != nullptr) {
			gridButtons[i]->updateFromAppearance(&g->gridAppearance, g->userName->getValue());
			gridButtons[i]->setToggleState(g->isCuelistOn->boolValue(), NotificationType::dontSendNotification);
		}
		else {
			gridButtons[i]->clear();
		}
		gridButtons[i]->repaint();
    }
}

void CuelistGridView::showContextMenu(int id)
{
    Cuelist* target = Brain::getInstance()->getCuelistById(id);
    PopupMenu p;
    if (target != nullptr) {
        p.addItem("Go", [target]() {target->userGo(); });
        p.addItem("Load", [target]() {target->showLoad(); });
        p.addItem("Load and go", [target]() {target->showLoadAndGo(); });
        if (target->cueA != nullptr) {
            p.addItem("Off", [target]() {target->off(); });
            p.addSeparator();
            p.addItem("Temp merge track", [target]() {target->tempMergeProgrammer(UserInputManager::getInstance()->getProgrammer(true), true); });
            p.addItem("Temp merge no track", [target]() {target->tempMergeProgrammer(UserInputManager::getInstance()->getProgrammer(true), false); });
        }
        p.addSeparator();
        p.addItem("Load content", [target]() {target->loadContent(UserInputManager::getInstance()->getProgrammer(true)); });
        p.addSeparator();
        p.addItem("Merge", [target]() {target->mergeWithProgrammer(UserInputManager::getInstance()->getProgrammer(true)); });
        p.addItem("Replace", [target]() {target->replaceWithProgrammer(UserInputManager::getInstance()->getProgrammer(true)); });
        p.addSeparator();
        p.addItem("Select as main conductor", [target]() {target->selectAsMainConductor(); });

    }
    else {
        p.addItem("Add", [id](){DataTransferManager::getInstance()->editObject("cuelist", id); });
    }
    p.showMenuAsync(PopupMenu::Options(), [this](int result) {});
}

void CuelistGridView::newMessage(const CuelistManager::ManagerEvent& e)
{
    updateCells();
}
