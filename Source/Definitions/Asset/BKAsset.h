/*
  ==============================================================================

    BKAsset.h
    Created: 8 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

class BKAsset :
    public BaseItem
{
public:
    BKAsset(var params = var());
    virtual ~BKAsset();

    String objectType;
    var objectData;

    IntParameter* id;
    int registeredId = 0;
    StringParameter* userName;

    FileParameter* imageFile;
    Image iconImage;

    void updateName();
    void loadImage();
    void copyImageToProjectLocal();
    bool hasValidImage() const { return iconImage.isValid(); }
    const Image& getImage() const { return iconImage; }

    void onContainerParameterChangedInternal(Parameter* p) override;
    InspectableEditor* getEditorInternal(bool isRoot, Array<Inspectable*> inspectables) override;

    String getTypeString() const override { return objectType; }
    static BKAsset* create(var params) { return new BKAsset(params); }

};


class BKAssetImagePreview :
    public Component
{
public:
    BKAssetImagePreview(BKAsset* asset);
    void paint(Graphics& g) override;
    BKAsset* item;
};


class BKAssetEditor :
    public BaseItemEditor
{
public:
    BKAssetEditor(BKAsset* item, bool isRoot);
    ~BKAssetEditor();

    BKAsset* assetItem;
    BKAssetImagePreview imagePreview;

    void resizedInternalContent(juce::Rectangle<int>& r) override;
    void controllableFeedbackUpdate(Controllable* c) override;
};
