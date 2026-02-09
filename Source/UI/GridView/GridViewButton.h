/*
  ==============================================================================

    GridButton.h
    Created: 9 Feb 2026
    Author:  j-mutter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class GridAppearance;

class GridViewButton :
    public TextButton,
	public DragAndDropContainer
{
public:
	GridViewButton();
	~GridViewButton();

    void updateFromAppearance(GridAppearance* a, String name);
    void clear();

    int id = 0;
    Image iconImage;

    void paint(juce::Graphics&) override;
	
	void mouseDown(const MouseEvent& e) override;
	void mouseUp(const MouseEvent& e) override;
	void mouseDrag(const MouseEvent& e) override;
};
