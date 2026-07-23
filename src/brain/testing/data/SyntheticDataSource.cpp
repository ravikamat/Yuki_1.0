#include "brain/testing/data/SyntheticDataSource.h"

namespace yuki {
namespace testing {

SyntheticDataSource::SyntheticDataSource(size_t totalSamples)
    : totalSamples_(totalSamples) {}

std::vector<float> SyntheticDataSource::fetchBatch(size_t batchSize) {
    std::vector<float> batch;
    for (size_t i = 0; i < batchSize && generated_ < totalSamples_; ++i) {
        batch.push_back(static_cast<float>(generated_) * 0.1f);
        generated_++;
    }
    return batch;
}

} // namespace testing
} // namespace yuki
