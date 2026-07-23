#ifndef YUKI_DATA_SOURCE_INTERFACE_H
#define YUKI_DATA_SOURCE_INTERFACE_H

#include <cstdint>
#include <vector>

namespace yuki {
namespace testing {

class DataSourceInterface {
public:
    virtual ~DataSourceInterface() = default;
    virtual std::vector<float> fetchBatch(size_t batchSize) = 0;
    virtual bool hasNext() const = 0;
};

} // namespace testing
} // namespace yuki

#endif
