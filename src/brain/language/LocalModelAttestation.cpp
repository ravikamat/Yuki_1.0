#include "src/brain/language/LocalModelAttestation.h"
#include <fstream>
#include <sstream>
#include <chrono>

namespace yuki::brain::language {

bool LocalModelAttestationRecord::isExpired(uint64_t nowUnixMs) const {
    if (expiresAtUnixMs == 0) return false;
    return nowUnixMs >= expiresAtUnixMs;
}

bool LocalModelAttestationRecord::matchesRuntime(
    const std::string& currentModelFingerprint,
    const std::string& currentServerFingerprint,
    const std::string& currentDeviceLuid) const {

    if (!modelFingerprintSha256.empty() && modelFingerprintSha256 != currentModelFingerprint) {
        return false;
    }
    if (!llamaServerFingerprintSha256.empty() && llamaServerFingerprintSha256 != currentServerFingerprint) {
        return false;
    }
    if (!deviceLuid.empty() && !currentDeviceLuid.empty() && deviceLuid != currentDeviceLuid) {
        return false;
    }
    return true;
}

std::string LocalModelAttestation::calculateFileFingerprint(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return "";

    // Simple FNV-1a 64-bit hash formatted as 16-hex string
    uint64_t hash = 14695981039346656037ULL;
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        for (std::streamsize i = 0; i < file.gcount(); ++i) {
            hash ^= static_cast<uint8_t>(buffer[i]);
            hash *= 1099511628211ULL;
        }
    }
    std::ostringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

LocalModelAttestationRecord LocalModelAttestation::load(const std::string& path) {
    LocalModelAttestationRecord record;
    std::ifstream file(path);
    if (!file.is_open()) return record;

    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    if (text.find("\"verified\": true") != std::string::npos || text.find("\"verified\":true") != std::string::npos) {
        record.verified = true;
    }
    if (text.find("\"schema_version\": 2") != std::string::npos || text.find("\"schema_version\":2") != std::string::npos) {
        record.schemaVersion = 2;
    }

    return record;
}

bool LocalModelAttestation::save(const LocalModelAttestationRecord& record, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"schema_version\": " << record.schemaVersion << ",\n";
    file << "  \"backend\": \"" << record.backend << "\",\n";
    file << "  \"status\": \"" << record.status << "\",\n";
    file << "  \"verified\": " << (record.verified ? "true" : "false") << ",\n";
    file << "  \"measured_at_unix_ms\": " << record.measuredAtUnixMs << ",\n";
    file << "  \"expires_at_unix_ms\": " << record.expiresAtUnixMs << ",\n";
    file << "  \"model_path\": \"" << record.modelPath << "\",\n";
    file << "  \"model_fingerprint_sha256\": \"" << record.modelFingerprintSha256 << "\",\n";
    file << "  \"llama_server_path\": \"" << record.llamaServerPath << "\",\n";
    file << "  \"llama_server_fingerprint_sha256\": \"" << record.llamaServerFingerprintSha256 << "\",\n";
    file << "  \"llama_bench_path\": \"" << record.llamaBenchPath << "\",\n";
    file << "  \"device_luid\": \"" << record.deviceLuid << "\",\n";
    file << "  \"device_name\": \"" << record.deviceName << "\",\n";
    file << "  \"prompt_tokens_per_second\": " << record.promptTokensPerSecond << ",\n";
    file << "  \"decode_tokens_per_second\": " << record.decodeTokensPerSecond << ",\n";
    file << "  \"raw_output_hash_sha256\": \"" << record.rawOutputHashSha256 << "\",\n";
    file << "  \"diagnostic\": \"" << record.diagnostic << "\"\n";
    file << "}\n";

    return true;
}

} // namespace yuki::brain::language
