/*
  ==============================================================================

    CarouselGridView.cpp
    Created: 19 Feb 2022 12:19:42am
    Author:  No

  ==============================================================================
*/

#include <JuceHeader.h>
#include "CarouselGridView.h"
#include "Brain.h"
#include "Definitions/Carousel/Carousel.h"
#include "Definitions/Carousel/CarouselManager.h"
#include "DataTransferManager/DataTransferManager.h"

//==============================================================================
CarouselGridViewUI::CarouselGridViewUI(const String& contentName):
    ShapeShifterContent(CarouselGridView::getInstance(), contentName)
{
    
}

CarouselGridViewUI::~CarouselGridViewUI()
{
}

juce_ImplementSingleton(CarouselGridView);

CarouselGridView::CarouselGridView()
{
    numberOfCells = 200;
    targetType = "Carousel";
    CarouselManager::getInstance()->addAsyncManagerListener(this);

}

CarouselGridView::~CarouselGridView()
{
    if (CarouselManager::getInstanceWithoutCreating() != nullptr) CarouselManager::getInstance()->removeAsyncManagerListener(this);
}

void CarouselGridView::updateCells() {
    for (int i = 0; i < numberOfCells; i++) {
        Carousel* g = Brain::getInstance()->getCarouselById(i+1);
		if (g != nullptr) {
			gridButtons[i]->updateFromAppearance(&g->gridAppearance, g->userName->getValue());
			gridButtons[i]->setToggleState(g->isCarouselOn->boolValue(), NotificationType::dontSendNotification);
		}
		else {
			gridButtons[i]->clear();
		}
		gridButtons[i]->repaint();
    }
}

void CarouselGridView::showContextMenu(int id)
{
    Carousel* target = Brain::getInstance()->getCarouselById(id);
    PopupMenu p;
    if (target != nullptr) {
        p.addItem("Start", [target]() {target->start(); });
        p.addItem("Stop", [target]() {target->stop(); });
    }
    else {
        p.addItem("Add", [id]() {DataTransferManager::getInstance()->editObject("carousel", id); });
    }
    p.showMenuAsync(PopupMenu::Options(), [this](int result) {});
}

void CarouselGridView::newMessage(const CarouselManager::ManagerEvent& e)
{
    updateCells();
}
