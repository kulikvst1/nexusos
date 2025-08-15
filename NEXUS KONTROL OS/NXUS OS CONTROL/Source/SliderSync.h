#pragma once

namespace SliderSync
{
    // ôëàãè äëÿ ïðåäîòâðàùåíèÿ ðèêóðñèâíûõ âûçîâîâ
    inline bool isMasterMoving = false;
    inline bool isChannelMoving = false;

    // ñîîòíîøåíèå L/R äëÿ ñîõðàíåíèÿ áàëàíñà
    inline float ratioL = 1.0f;
    inline float ratioR = 1.0f;
}
