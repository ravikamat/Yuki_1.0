#include "StateSerializer.h"
#include "brain/security/SecuritySandbox.h"
#include <fstream>
#include <sstream>
#include <chrono>

namespace yuki {
namespace persistence {

StateBundle::StateBundle() {
    using namespace std::chrono;
    header_.timestamp_ms = static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()
    );
}

void StateBundle::addChunk(uint32_t component_id, const std::string& name,
                           const std::string& data) {
    StateChunk chunk;
    chunk.component_id = component_id;
    chunk.component_name = name;
    chunk.data = data;
    chunks_.push_back(std::move(chunk));
    header_.component_count = static_cast<uint32_t>(chunks_.size());
}

std::string StateBundle::serialize() const {
    std::ostringstream oss;
    oss.write(reinterpret_cast<const char*>(&header_.magic), sizeof(header_.magic));
    oss.write(reinterpret_cast<const char*>(&header_.version), sizeof(header_.version));
    oss.write(reinterpret_cast<const char*>(&header_.timestamp_ms), sizeof(header_.timestamp_ms));
    oss.write(reinterpret_cast<const char*>(&header_.component_count), sizeof(header_.component_count));

    for (const auto& c : chunks_) {
        uint32_t name_len = static_cast<uint32_t>(c.component_name.size());
        uint32_t data_len = static_cast<uint32_t>(c.data.size());

        oss.write(reinterpret_cast<const char*>(&c.component_id), sizeof(c.component_id));
        oss.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        oss.write(c.component_name.data(), name_len);
        oss.write(reinterpret_cast<const char*>(&data_len), sizeof(data_len));
        oss.write(c.data.data(), data_len);
    }
    return oss.str();
}

bool StateBundle::deserialize(const std::string& data) {
    if (data.size() < sizeof(StateBundleHeader)) return false;

    std::istringstream iss(data);

    iss.read(reinterpret_cast<char*>(&header_.magic), sizeof(header_.magic));
    iss.read(reinterpret_cast<char*>(&header_.version), sizeof(header_.version));
    iss.read(reinterpret_cast<char*>(&header_.timestamp_ms), sizeof(header_.timestamp_ms));
    iss.read(reinterpret_cast<char*>(&header_.component_count), sizeof(header_.component_count));

    if (header_.magic != 0x59554B49) return false;
    if (header_.version != 1) return false;

    chunks_.clear();
    for (uint32_t i = 0; i < header_.component_count; ++i) {
        StateChunk c;
        uint32_t name_len = 0, data_len = 0;

        iss.read(reinterpret_cast<char*>(&c.component_id), sizeof(c.component_id));
        iss.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        c.component_name.resize(name_len);
        iss.read(c.component_name.data(), name_len);
        iss.read(reinterpret_cast<char*>(&data_len), sizeof(data_len));
        c.data.resize(data_len);
        iss.read(c.data.data(), data_len);

        if (!iss) return false;
        chunks_.push_back(std::move(c));
    }
    return true;
}

bool StateBundle::isValid() const noexcept {
    return header_.magic == 0x59554B49 && header_.version == 1;
}

int StateSerializer::write(const std::string& validated_path,
                            const StateBundle& bundle) {
    auto& sandbox = security::SecuritySandbox::instance();
    auto verdict = sandbox.validateWrite(validated_path);
    if (!verdict.allowed()) {
        return ERR_SANDBOX_DENY;
    }

    std::ofstream ofs(validated_path, std::ios::binary);
    if (!ofs) return ERR_IO_WRITE;

    auto data = bundle.serialize();
    ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!ofs) return ERR_IO_WRITE;
    return ERR_SUCCESS;
}

int StateSerializer::read(const std::string& validated_path,
                           StateBundle* out_bundle) {
    if (!out_bundle) return ERR_CORRUPT;

    auto& sandbox = security::SecuritySandbox::instance();
    auto verdict = sandbox.validateRead(validated_path);
    if (!verdict.allowed()) {
        return ERR_SANDBOX_DENY;
    }

    std::ifstream ifs(validated_path, std::ios::binary);
    if (!ifs) return ERR_IO_READ;

    std::string data((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
    if (!ifs && !ifs.eof()) return ERR_IO_READ;

    if (!out_bundle->deserialize(data)) {
        return ERR_CORRUPT;
    }
    return ERR_SUCCESS;
}

} // namespace persistence
} // namespace yuki
