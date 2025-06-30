// Grid.h
#pragma once
#include <JuceHeader.h>

struct Grid
{
    Grid(juce::Rectangle<int> areaToSplit,
        int numCols,
        int numRows) noexcept
        : area(areaToSplit), cols(numCols), rows(numRows)
    {}

    // ��������� ������ �� ������� 0�cols*rows-1
    juce::Rectangle<int> getSector(int index) const noexcept
    {
        int c = index % cols;
        int r = index / cols;
        int w = area.getWidth() / cols;
        int h = area.getHeight() / rows;
        return { area.getX() + c * w,
                 area.getY() + r * h,
                 w, h };
    }

    // ����������� ���� �� start �� end (������������)
    juce::Rectangle<int> getUnion(int start, int end) const noexcept
    {
        return getSector(start).getUnion(getSector(end));
    }

private:
    juce::Rectangle<int> area;
    int cols, rows;
};
