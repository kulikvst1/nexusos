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

    // åäèíè÷íàÿ ÿ÷åéêà ïî èíäåêñó 0…cols*rows-1
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

    // ïåðåãðóçêà: ïî íîìåðó ñòðîêè è ñòîëáöà
    juce::Rectangle<int> getSector(int row, int col) const noexcept
    {
        return getSector(row * cols + col);
    }

    // îáúåäèí¸ííàÿ çîíà îò start äî end (âêëþ÷èòåëüíî)
    juce::Rectangle<int> getUnion(int start, int end) const noexcept
    {
        return getSector(start).getUnion(getSector(end));
    }

    // ïåðåãðóçêà: îáúåäèíåíèå â ïðåäåëàõ îäíîé ñòðîêè
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
