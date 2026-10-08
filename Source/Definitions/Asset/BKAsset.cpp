/*
  ==============================================================================

    BKAsset.cpp
    Created: 8 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#include "BKAsset.h"
#include "../../Brain.h"
#include "BKAssetManager.h"

BKAsset::BKAsset(var params) :
	BaseItem(params.getProperty("name", "Asset")),
	objectType(params.getProperty("type", "Asset").toString()),
	objectData(params)
{
	saveAndLoadRecursiveData = true;
	editorIsCollapsed = false;
	itemDataType = "Asset";
	nameCanBeChangedByUser = false;

	id = addIntParameter("ID", "ID of this asset", 1, 1);
	userName = addStringParameter("Name", "Name of this asset", "New asset");
	imageFile = addFileParameter("Image File", "Image file (PNG, JPG, etc.)");
	imageFile->setForceRelativePath(true);

	updateName();

	Brain::getInstance()->registerAsset(this, id->getValue());
}

BKAsset::~BKAsset()
{
	Brain::getInstance()->unregisterAsset(this);
}

void BKAsset::updateName()
{
	String n = userName->getValue();
	if (parentContainer != nullptr) {
		dynamic_cast<BKAssetManager*>(parentContainer.get())->reorderItems();
	}
	setNiceName(String((int)id->getValue()) + " - " + n);
}

void BKAsset::copyImageToProjectLocal()
{
	File srcFile = imageFile->getFile();
	if (!srcFile.existsAsFile()) return;

	File projectFile = Engine::mainEngine->getFile();
	if (!projectFile.existsAsFile()) return;

	File assetsDir = projectFile.getParentDirectory().getChildFile("assets");
	if (!assetsDir.exists()) {
		assetsDir.createDirectory();
	}

	File destFile = assetsDir.getChildFile(srcFile.getFileName());

	// If already in the assets dir, no need to copy
	if (srcFile.getParentDirectory() == assetsDir) return;

	// Handle filename collisions
	if (destFile.existsAsFile()) {
		String baseName = srcFile.getFileNameWithoutExtension();
		String ext = srcFile.getFileExtension();
		int counter = 1;
		while (destFile.existsAsFile()) {
			destFile = assetsDir.getChildFile(baseName + "_" + String(counter) + ext);
			counter++;
		}
	}

	if (srcFile.copyFileTo(destFile)) {
		// Update the parameter to point to the local copy
		imageFile->setValue(destFile.getFullPathName());
	}
}

void BKAsset::loadImage()
{
	File f = imageFile->getFile();
	if (f.existsAsFile()) {
		iconImage = ImageFileFormat::loadFrom(f);
		if (iconImage.isValid()) {
			int maxDim = 128;
			if (iconImage.getWidth() > maxDim || iconImage.getHeight() > maxDim) {
				float scale = jmin((float)maxDim / iconImage.getWidth(), (float)maxDim / iconImage.getHeight());
				iconImage = iconImage.rescaled((int)(iconImage.getWidth() * scale), (int)(iconImage.getHeight() * scale));
			}
		}
	}
	else {
		iconImage = Image();
	}
}

void BKAsset::onContainerParameterChangedInternal(Parameter* p)
{
	if (p == id) {
		Brain::getInstance()->registerAsset(this, id->getValue());
	}
	if (p == userName || p == id) {
		updateName();
	}
	if (p == imageFile) {
		copyImageToProjectLocal();
		loadImage();
	}
}

InspectableEditor* BKAsset::getEditorInternal(bool isRoot, Array<Inspectable*> inspectables)
{
	return new BKAssetEditor(this, isRoot);
}


// ============== Image Preview Component ==============

BKAssetImagePreview::BKAssetImagePreview(BKAsset* asset) :
	item(asset)
{
}

void BKAssetImagePreview::paint(Graphics& g)
{
	g.fillAll(Colour(30, 30, 30));

	if (item != nullptr && item->hasValidImage()) {
		const Image& img = item->getImage();
		auto bounds = getLocalBounds().reduced(4);

		float imgAspect = (float)img.getWidth() / img.getHeight();
		float boundsAspect = (float)bounds.getWidth() / bounds.getHeight();
		int drawW, drawH;
		if (imgAspect > boundsAspect) {
			drawW = bounds.getWidth();
			drawH = (int)(drawW / imgAspect);
		}
		else {
			drawH = bounds.getHeight();
			drawW = (int)(drawH * imgAspect);
		}
		int drawX = bounds.getX() + (bounds.getWidth() - drawW) / 2;
		int drawY = bounds.getY() + (bounds.getHeight() - drawH) / 2;
		g.drawImage(img, drawX, drawY, drawW, drawH, 0, 0, img.getWidth(), img.getHeight());
	}
	else {
		g.setColour(Colours::grey);
		g.drawFittedText("No image loaded", getLocalBounds(), Justification::centred, 1);
	}

	g.setColour(Colour(60, 60, 60));
	g.drawRect(getLocalBounds());
}


// ============== BKAsset Editor ==============

BKAssetEditor::BKAssetEditor(BKAsset* item, bool isRoot) :
	BaseItemEditor({ item }, isRoot),
	assetItem(item),
	imagePreview(item)
{
	addAndMakeVisible(imagePreview);
	resized();
}

BKAssetEditor::~BKAssetEditor()
{
}

void BKAssetEditor::resizedInternalContent(juce::Rectangle<int>& r)
{
	BaseItemEditor::resizedInternalContent(r);

	int previewHeight = 100;
	imagePreview.setBounds(r.withHeight(previewHeight));
	r.translate(0, previewHeight + 4);
}

void BKAssetEditor::controllableFeedbackUpdate(Controllable* c)
{
	if (c == assetItem->imageFile) {
		imagePreview.repaint();
	}
}
