#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

struct Camera {
    float x = 0.0f;
    float z = 0.0f;
    float span = 96.0f;
    static constexpr float initialYaw = DirectX::XM_PIDIV4;
    float yaw = initialYaw;

    void rotate(float radians) { yaw = std::remainder(yaw + radians, DirectX::XM_2PI); }
    DirectX::XMFLOAT3 right() const { return {-std::sin(yaw), 0, std::cos(yaw)}; }
    DirectX::XMFLOAT3 up() const {
        const float elevation = std::atan2(78.0f, 91.9238816f);
        return {-std::sin(elevation) * std::cos(yaw), std::cos(elevation), -std::sin(elevation) * std::sin(yaw)};
    }
    unsigned spriteDirection(float heading) const {
        const int direction = static_cast<int>(std::lround(std::remainder(heading - yaw, DirectX::XM_2PI) / DirectX::XM_PIDIV4));
        return static_cast<unsigned>((direction % 8 + 8) % 8);
    }

    void zoom(float steps) { span = std::clamp(span * std::pow(0.85f, steps), 24.0f, 160.0f); }
    void pan(float right, float forward, float seconds) {
        const float speed = span * 0.55f * seconds;
        x = std::clamp(x + (-right * std::sin(yaw) - forward * std::cos(yaw)) * speed, -55.0f, 55.0f);
        z = std::clamp(z + (right * std::cos(yaw) - forward * std::sin(yaw)) * speed, -55.0f, 55.0f);
    }
    DirectX::XMMATRIX matrix(float aspect) const {
        using namespace DirectX;
        const auto target = XMVectorSet(x, 0.0f, z, 1.0f);
        const auto eye = XMVectorAdd(target, XMVectorSet(91.9238816f * std::cos(yaw), 78.0f, 91.9238816f * std::sin(yaw), 0.0f));
        return XMMatrixLookAtLH(eye, target, XMVectorSet(0, 1, 0, 0)) *
            XMMatrixOrthographicLH(span, span / aspect, 1.0f, 300.0f);
    }
};
