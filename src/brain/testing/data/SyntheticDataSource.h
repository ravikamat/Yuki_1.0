#ifndef YUKI_SYNTHETIC_DATA_SOURCE_H
#define YUKI_SYNTHETIC_DATA_SOURCE_H

#include "brain/testing/data/DataSourceInterface.h"

namespace yuki {
namespace testing {

class SyntheticDataSource : public DataSourceInterface {
public:
    explicit SyntheticDataSource(size_t totalSamples = 100);

    std::vector<float> fetchBatch(size_t batchSize) override;
    bool hasNext() const override { return generated_ < totalSamples_; }

private:
    size_t totalSamples_;
    size_t generated_ = 0;
};

} // namespace testing
} // namespace yuki

#endif
