#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

struct Camera {
    float x = 0.0f;
    float z = 0.0f;
    float span = 96.0f;

    void zoom(float steps) { span = std::clamp(span * std::pow(0.85f, steps), 24.0f, 160.0f); }
    void pan(float right, float forward, float seconds) {
        const float speed = span * 0.55f * seconds * 0.70710678f;
        x = std::clamp(x + (-right - forward) * speed, -55.0f, 55.0f);
        z = std::clamp(z + (right - forward) * speed, -55.0f, 55.0f);
    }
    DirectX::XMMATRIX matrix(float aspect) const {
        using namespace DirectX;
        const auto target = XMVectorSet(x, 0.0f, z, 1.0f);
        const auto eye = XMVectorAdd(target, XMVectorSet(65.0f, 78.0f, 65.0f, 0.0f));
        return XMMatrixLookAtLH(eye, target, XMVectorSet(0, 1, 0, 0)) *
            XMMatrixOrthographicLH(span, span / aspect, 1.0f, 300.0f);
    }
};
