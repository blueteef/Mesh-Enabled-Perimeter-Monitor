// Axis mapping: which chip axis points forward / right / up on the finished unit.
// Spec string "f,r,u" where each token is x, y or z with an optional '-' sign,
// e.g. "y,x,z" = chip +Y points forward, chip +X points right, chip +Z points up.
// (Each chip axis must be used exactly once, and the result must be physically
// possible: forward x right must point opposite to up, i.e. a right-handed frame.)
#pragma once
#include <Arduino.h>

namespace axes {

struct Map {
    int8_t idx[3] = {0, 1, 2};   // chip axis index for forward, right, up
    int8_t sgn[3] = {1, -1, 1};
    bool apply(const float in[3], float out[3]) const {
        for (int i = 0; i < 3; ++i) out[i] = sgn[i] * in[idx[i]];
        return true;
    }
};

// Returns true and fills m if the spec is valid.
inline bool parse(const char* spec, Map& m) {
    Map t; bool used[3] = {false, false, false};
    const char* p = spec; int n = 0;
    while (*p && n < 3) {
        while (*p == ' ' || *p == ',') ++p;
        int s = 1;
        if (*p == '-') { s = -1; ++p; } else if (*p == '+') ++p;
        char c = *p | 0x20;
        if (c < 'x' || c > 'z') return false;
        int i = c - 'x';
        if (used[i]) return false;
        used[i] = true; t.idx[n] = i; t.sgn[n] = s; ++n; ++p;
    }
    if (n != 3) return false;
    // handedness: det of rows (F, R, U) must be -1  (F x R = down = -U)
    float M[3][3] = {};
    for (int r = 0; r < 3; ++r) M[r][t.idx[r]] = t.sgn[r];
    float det = M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
              - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0])
              + M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
    if (det > -0.5f) return false;
    m = t; return true;
}

}  // namespace axes
