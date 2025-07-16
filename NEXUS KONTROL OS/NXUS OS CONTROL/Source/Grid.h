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

    // единичная ячейка по индексу 0…cols*rows-1
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

    // перегрузка: по номеру строки и столбца
    juce::Rectangle<int> getSector(int row, int col) const noexcept
    {
        return getSector(row * cols + col);
    }

    // объединённая зона от start до end (включительно)
    juce::Rectangle<int> getUnion(int start, int end) const noexcept
    {
        return getSector(start).getUnion(getSector(end));
    }

    // перегрузка: объединение в пределах одной строки
    juce::Rectangle<int> getUnion(int row,
        int colStart,
        int colEnd) const noexcept
    {
        return getUnion(row * cols + colStart,
            row * cols + colEnd);
    }

private:
    juce::Rectangle<int> area;
    int cols, rows;
};
