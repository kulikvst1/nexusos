#pragma once
#include "IPitchDetector.h"
#include <vector>
#include <cmath>
#include <algorithm>

class MPMDetector : public IPitchDetector
{
public:
    void prepare(int bufferSize, double sampleRate) override
    {
        N = bufferSize; fs = sampleRate;
        nsdf.assign(N, 0.0f);
    }

    PitchResult process(const float* x, int size) override
    {
        if (size != N) return { -1.0f, 0.0f };

        // NSDF (íîðìèðîâàííàÿ àâòîêîððåëÿöèÿ ðàçíîñòè)
        const int maxTau = N - 1;
        nsdf[0] = 1.0f;
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            double ac = 0.0, m0 = 0.0, m1 = 0.0;
            const int M = N - tau;
            for (int i = 0; i < M; ++i)
            {
                const float x0 = x[i];
                const float x1 = x[i + tau];
                ac += (double)x0 * x1;
                m0 += (double)x0 * x0;
                m1 += (double)x1 * x1;
            }
            const double denom = std::sqrt(std::max(1e-20, m0 * m1));
            nsdf[tau] = (float)((denom > 0.0) ? (ac / denom) : 0.0);
        }

        // Ïîèñê ïåðâîãî ëîêàëüíîãî ìàêñèìóìà âûøå ïîðîãà
        const float thr = 0.6f; // ìîæíî 0.55–0.7
        int bestTau = -1; float bestVal = -1.0f;
        for (int tau = 2; tau < maxTau - 1; ++tau)
        {
            const float y0 = nsdf[tau - 1], y1 = nsdf[tau], y2 = nsdf[tau + 1];
            if (y1 > y0 && y1 >= y2 && y1 > thr) { bestTau = tau; bestVal = y1; break; }
        }
        if (bestTau < 0)
        {
            for (int tau = 2; tau < maxTau - 1; ++tau)
                if (nsdf[tau] > bestVal) { bestVal = nsdf[tau]; bestTau = tau; }
            if (bestTau < 0 || bestVal < 0.2f) return { -1.0f, 0.0f };
        }

        // Ïàðàáîëè÷åñêàÿ èíòåðïîëÿöèÿ ìàêñèìóìa NSDF
        float tauF = (float)bestTau;
        if (bestTau > 1 && bestTau < maxTau)
        {
            const float y1 = nsdf[bestTau - 1];
            const float y2 = nsdf[bestTau];
            const float y3 = nsdf[bestTau + 1];
            const float denom = (2.0f * (y1 - 2.0f * y2 + y3));
            const float delta = (std::abs(denom) > 1e-12f) ? (y1 - y3) / denom : 0.0f;
            tauF += delta;
        }

        const float hz = (tauF > 0.0f) ? (float)(fs / tauF) : -1.0f;
        const float conf = std::clamp(bestVal, 0.0f, 1.0f); // ïèê NSDF êàê äîâåðèå
        if (!(hz >= 70.0f && hz <= 1200.0f)) return { -1.0f, 0.0f };
        return { hz, conf };
    }

private:
    int N = 0; double fs = 0.0;
    std::vector<float> nsdf;
};

