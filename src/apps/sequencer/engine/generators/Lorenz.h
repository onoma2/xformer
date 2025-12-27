#pragma once

#include "core/math/Math.h"

class Lorenz {
public:
    Lorenz() {
        reset();
    }

    void reset() {
        _x = 0.1f;
        _y = 0.0f;
        _z = 0.0f;
    }

    /**
     * Advances the Lorenz attractor using Euler integration.
     * Designed for 1kHz update rate (1ms).
     * 
     * @param dt Time step (e.g., 0.001 for 1ms). Multiplied by speed factor.
     * @param rho The "Rayleigh number" (Chaos parameter). Typical value ~28.
     *            Mapped from P1 (0-100) -> 10.0 to 50.0
     * @param beta The geometric factor. Typical value 8/3 (~2.66).
     *             Mapped from P2 (0-100) -> 0.5 to 4.0
     * @return Bipolar output (-1.0 to 1.0) derived from X state.
     */
    float next(float dt, float rho, float beta) {
        // Lorenz Constants
        const float sigma = 10.0f; // Prandtl number (usually constant)

        // Equations:
        // dx = sigma * (y - x)
        // dy = x * (rho - z) - y
        // dz = x * y - beta * z

        float dx = sigma * (_y - _x);
        float dy = _x * (rho - _z) - _y;
        float dz = _x * _y - beta * _z;

        // Euler Integration
        _x += dx * dt;
        _y += dy * dt;
        _z += dz * dt;

        // Normalization
        // Lorenz X typically swings between -20 and +20 for standard params (rho=28).
        // At max rho=50, it can reach ~27.
        // We scale by 0.045 to keep the main action in -1..1 range while allowing 
        // occasional overshoots/clipping at extreme settings for more energy.
        return clamp(_x * 0.045f, -1.0f, 1.0f);
    }

private:
    float _x;
    float _y;
    float _z;
};
