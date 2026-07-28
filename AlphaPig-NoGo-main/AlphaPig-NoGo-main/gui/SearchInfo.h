#pragma once
#include <vector>

struct SearchInfo
{
    int   playouts      = 0;
    double thinkingTime  = 0.0;
    float value      = 0.0f;
    int   bestMove   = -1;
    int   bestRow    = -1;
    int   bestCol    = -1;
    int   totalMoves = 0;
    bool  resigned   = false;

    std::vector<float> policy;
    std::vector<int>   visits;

    SearchInfo() { policy.resize(81, 0.f); visits.resize(81, 0); }
};
