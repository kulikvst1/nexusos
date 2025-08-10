#pragma once

namespace SliderSync
{
    // флаги для предотвращения рикурсивных вызовов
    inline bool isMasterMoving = false;
    inline bool isChannelMoving = false;

    // соотношение L/R для сохранения баланса
    inline float ratioL = 1.0f;
    inline float ratioR = 1.0f;
}
