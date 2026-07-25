#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <memory>

namespace yuki { namespace self {

class SelfModel;
class TheoryOfMind;
class ValenceArousalModel;
class ConfidenceCalibrator;

struct IdentitySnapshot {
    uint64_t id = 0;
    uint64_t timestamp = 0;
    std::string version;
    std::vector<uint8_t> selfModelBlob;
    std::vector<uint8_t> theoryOfMindBlob;
    std::vector<uint8_t> valenceArousalBlob;
    std::vector<uint8_t> confidenceCalibratorBlob;
    uint64_t previousHash = 0;
    uint64_t currentHash = 0;
    double identityDrift = 0.0;
};

struct IdentityTimelineEntry {
    uint64_t snapshotId = 0;
    uint64_t timestamp = 0;
    std::string metricName;
    double metricValue = 0.0;
};

struct AutobiographicalEntry {
    uint64_t id = 0;
    uint64_t timestamp = 0;
    std::string entryType;
    std::vector<uint8_t> contentBlob;
    uint64_t relatedSnapshotId = 0;
};

class IdentityPersistence {
public:
    explicit IdentityPersistence(const std::string& dbPath);
    ~IdentityPersistence();
    IdentityPersistence(const IdentityPersistence&) = delete;
    IdentityPersistence& operator=(const IdentityPersistence&) = delete;
    IdentityPersistence(IdentityPersistence&&) noexcept;
    IdentityPersistence& operator=(IdentityPersistence&&) noexcept;

    bool initializeSchema();

    bool saveIdentity(const SelfModel& self, const TheoryOfMind& tom,
                      const ValenceArousalModel& emotion, const ConfidenceCalibrator& calibrator,
                      const std::string& version = "1.0.0");

    bool loadLatestIdentity(SelfModel& self, TheoryOfMind& tom,
                            ValenceArousalModel& emotion, ConfidenceCalibrator& calibrator);

    bool loadIdentityById(uint64_t snapshotId, SelfModel& self, TheoryOfMind& tom,
                          ValenceArousalModel& emotion, ConfidenceCalibrator& calibrator);

    std::vector<IdentityTimelineEntry> getTimeline(uint64_t startTime, uint64_t endTime);
    std::vector<IdentitySnapshot> getSnapshotHistory(size_t limit = 100);

    bool addAutobiographicalEntry(const std::string& entryType,
                                  const std::vector<uint8_t>& content,
                                  uint64_t relatedSnapshotId = 0);
    std::vector<AutobiographicalEntry> getAutobiographicalEntries(size_t limit = 100);
    std::string generateNarrativeSummary();

    double computeDriftBetween(uint64_t snapshotIdA, uint64_t snapshotIdB);
    double computeLatestDrift();
    bool verifyHashChain();
    uint64_t getLatestHash() const;

    size_t getSnapshotCount() const;
    size_t getEntryCount() const;
    bool pruneSnapshots(size_t keepLast);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    uint64_t computeFNV1a(const std::vector<uint8_t>& data, uint64_t previousHash = 0) const;
};

}} // namespace yuki::self
