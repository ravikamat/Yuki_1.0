#include "CapabilityProfile.h"
#include <cstring>

using namespace yuki::capability;

std::vector<uint8_t> CapabilityProfile::serialize() const {
    std::vector<uint8_t> data;
    auto append_str = [&data](const std::string& s) {
        uint32_t len = static_cast<uint32_t>(s.size());
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&len), 
                    reinterpret_cast<uint8_t*>(&len) + sizeof(len));
        data.insert(data.end(), s.begin(), s.end());
    };
    auto append_vec_str = [&data, &append_str](const std::vector<std::string>& v) {
        uint32_t count = static_cast<uint32_t>(v.size());
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&count),
                    reinterpret_cast<uint8_t*>(&count) + sizeof(count));
        for (const auto& s : v) append_str(s);
    };
    auto append_f32 = [&data](float f) {
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&f),
                    reinterpret_cast<uint8_t*>(&f) + sizeof(f));
    };
    auto append_bool = [&data](bool b) {
        uint8_t v = b ? 1 : 0;
        data.push_back(v);
    };

    append_str(tool_id);
    append_vec_str(inputs);
    append_vec_str(outputs);
    append_f32(avg_duration_ms);
    append_f32(avg_ram_mb);
    append_f32(avg_cpu_percent);
    append_f32(base_risk);
    append_f32(required_competence);
    append_vec_str(platform_tags);
    append_bool(produces_artifacts);
    append_bool(is_destructive);
    return data;
}

static std::string read_str(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + sizeof(uint32_t) > end) return {};
    uint32_t len = 0;
    std::memcpy(&len, ptr, sizeof(len));
    ptr += sizeof(len);
    if (ptr + len > end) return {};
    std::string s(reinterpret_cast<const char*>(ptr), len);
    ptr += len;
    return s;
}

static std::vector<std::string> read_vec_str(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + sizeof(uint32_t) > end) return {};
    uint32_t count = 0;
    std::memcpy(&count, ptr, sizeof(count));
    ptr += sizeof(count);
    std::vector<std::string> v;
    v.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        v.push_back(read_str(ptr, end));
    }
    return v;
}

static float read_f32(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + sizeof(float) > end) return 0.0f;
    float f = 0.0f;
    std::memcpy(&f, ptr, sizeof(f));
    ptr += sizeof(f);
    return f;
}

static bool read_bool(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr + 1 > end) return false;
    bool b = (*ptr != 0);
    ++ptr;
    return b;
}

std::optional<CapabilityProfile> CapabilityProfile::deserialize(const std::vector<uint8_t>& data) {
    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();
    CapabilityProfile p;
    p.tool_id = read_str(ptr, end);
    p.inputs = read_vec_str(ptr, end);
    p.outputs = read_vec_str(ptr, end);
    p.avg_duration_ms = read_f32(ptr, end);
    p.avg_ram_mb = read_f32(ptr, end);
    p.avg_cpu_percent = read_f32(ptr, end);
    p.base_risk = read_f32(ptr, end);
    p.required_competence = read_f32(ptr, end);
    p.platform_tags = read_vec_str(ptr, end);
    p.produces_artifacts = read_bool(ptr, end);
    p.is_destructive = read_bool(ptr, end);
    if (ptr != end) return std::nullopt; // trailing bytes = corruption
    return p;
}
