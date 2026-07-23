#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace yuki {
namespace persistence {

struct StateBundleHeader {
    uint32_t magic = 0x59554B49; // "YUKI"
    uint32_t version = 1;
    uint64_t timestamp_ms = 0;
    uint32_t component_count = 0;
};

struct StateChunk {
    uint32_t component_id = 0;
    std::string component_name;
    std::string data;
};

class StateBundle {
public:
    StateBundle();
    void addChunk(uint32_t component_id, const std::string& name,
                  const std::string& data);
    const std::vector<StateChunk>& chunks() const noexcept { return chunks_; }
    std::string serialize() const;
    bool deserialize(const std::string& data);
    bool isValid() const noexcept;

private:
    StateBundleHeader header_;
    std::vector<StateChunk> chunks_;
};

class StateSerializer {
public:
    static int write(const std::string& validated_path,
                     const StateBundle& bundle);
    static int read(const std::string& validated_path,
                    StateBundle* out_bundle);

    static constexpr int ERR_SUCCESS = 0;
    static constexpr int ERR_SANDBOX_DENY = 1;
    static constexpr int ERR_IO_WRITE = 2;
    static constexpr int ERR_IO_READ = 3;
    static constexpr int ERR_CORRUPT = 4;
    static constexpr int ERR_VERSION = 5;
};

} // namespace persistence
} // namespace yuki
