// SpatialAnchor.cpp
#include "SpatialAnchor.h"
#include <algorithm>

namespace yuki::perception {

SpatialAnchor SpatialAnchor::fuse(const SpatialAnchor& a, const SpatialAnchor& b) {
    SpatialAnchor result;
    result.reference_frame = (a.reference_frame == b.reference_frame) ? a.reference_frame : "fused";
    double prec_a = a.uncertainty.confidence / (a.uncertainty.volume() + 1e-6);
    double prec_b = b.uncertainty.confidence / (b.uncertainty.volume() + 1e-6);
    double total_prec = prec_a + prec_b;
    if (total_prec < 1e-6) {
        result.pose.x = (a.pose.x + b.pose.x) * 0.5;
        result.pose.y = (a.pose.y + b.pose.y) * 0.5;
        result.pose.z = (a.pose.z + b.pose.z) * 0.5;
    } else {
        result.pose.x = (a.pose.x * prec_a + b.pose.x * prec_b) / total_prec;
        result.pose.y = (a.pose.y * prec_a + b.pose.y * prec_b) / total_prec;
        result.pose.z = (a.pose.z * prec_a + b.pose.z * prec_b) / total_prec;
    }
    result.uncertainty.sigma_x = std::sqrt(a.uncertainty.sigma_x*a.uncertainty.sigma_x + b.uncertainty.sigma_x*b.uncertainty.sigma_x);
    result.uncertainty.sigma_y = std::sqrt(a.uncertainty.sigma_y*a.uncertainty.sigma_y + b.uncertainty.sigma_y*b.uncertainty.sigma_y);
    result.uncertainty.sigma_z = std::sqrt(a.uncertainty.sigma_z*a.uncertainty.sigma_z + b.uncertainty.sigma_z*b.uncertainty.sigma_z);
    result.uncertainty.confidence = std::max(a.uncertainty.confidence, b.uncertainty.confidence);
    return result;
}

} // namespace yuki::perception
