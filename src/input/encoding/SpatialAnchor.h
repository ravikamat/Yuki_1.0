#pragma once
// SpatialAnchor.h
// Yuki_1.0 — Observation Encoder

#include <cmath>
#include <optional>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace yuki::perception {

struct EgocentricPose {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double yaw = 0.0;
    double pitch = 0.0;
    double roll = 0.0;
    double distance() const { return std::sqrt(x*x + y*y + z*z); }
};

struct SpatialUncertainty {
    double sigma_x = 0.1;
    double sigma_y = 0.1;
    double sigma_z = 0.1;
    double confidence = 1.0;
    double volume() const { return (4.0/3.0) * M_PI * sigma_x * sigma_y * sigma_z; }
};

struct SpatialAnchor {
    EgocentricPose pose;
    SpatialUncertainty uncertainty;
    std::string reference_frame;
    static SpatialAnchor fuse(const SpatialAnchor& a, const SpatialAnchor& b);
};

} // namespace yuki::perception
