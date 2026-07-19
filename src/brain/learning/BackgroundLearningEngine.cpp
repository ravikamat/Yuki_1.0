#include "BackgroundLearningEngine.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "input/encoding/ObservationEncoder.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/learning/KnowledgeDaemon.h"
#include "infrastructure/CoreBus.h"
#include "infrastructure/ModuleRegistry.h"
#include <iostream>
#include <chrono>

using namespace yuki::learning;

BackgroundLearningEngine::BackgroundLearningEngine() = default;
BackgroundLearningEngine::~BackgroundLearningEngine() { stop(); }

void BackgroundLearningEngine::init(
    std::shared_ptr<yuki::memory::CognitiveMemoryFabric> cmf,
    std::shared_ptr<yuki::perception::TextEncoder> encoder,
    yuki::inference::VariationalStateEstimator* vse)
{
    cmf_     = cmf;
    encoder_ = encoder;
    vse_     = vse;
}

void BackgroundLearningEngine::start() {
    if (running_.load()) return;
    running_ = true;
    thread_  = std::thread(&BackgroundLearningEngine::loop, this);
    std::cout << "[BLE] BackgroundLearningEngine started.\n";
}

void BackgroundLearningEngine::stop() {
    running_ = false;
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void BackgroundLearningEngine::ingest(LearningSample sample) {
    {
        std::lock_guard<std::mutex> lock(queue_mtx_);
        queue_.push(std::move(sample));
    }
    cv_.notify_one();
}

void BackgroundLearningEngine::ingestUserTurn(const std::string& text) {
    LearningSample sample;
    sample.text      = text;
    sample.source    = "user_turn";
    sample.topic     = "general";
    sample.timestamp = std::chrono::system_clock::now();
    // Extract 8D text embedding features
    if (encoder_) {
        sample.features = encoder_->encode(text);
        sample.label_confidence = 0.6f;
    }
    ingest(std::move(sample));
}

void BackgroundLearningEngine::setCurriculumTopics(const std::vector<std::string>& topics) {
    curriculum_topics_ = topics;
    curriculum_index_  = 0;
}

void BackgroundLearningEngine::loop() {
    while (running_) {
        LearningSample sample;
        {
            std::unique_lock<std::mutex> lock(queue_mtx_);
            cv_.wait_for(lock, std::chrono::seconds(2), [this] {
                return !queue_.empty() || !running_;
            });
            if (!running_) break;
            if (queue_.empty()) {
                // Idle — generate synthetic curriculum sample
                lock.unlock();
                sample = generateCurriculumSample();
            } else {
                sample = std::move(queue_.front());
                queue_.pop();
            }
        }
        processSample(sample);

        // ── Drain web-scraped knowledge packets from KnowledgeDaemon ───────
        if (knowledge_daemon_) {
            auto packets = knowledge_daemon_->getRecentPackets(2);
            for (const auto& kp : packets) {
                if (kp.summary.empty()) continue;
                LearningSample web_sample;
                web_sample.text      = kp.summary;
                web_sample.source    = "web";
                web_sample.topic     = kp.topic;
                web_sample.timestamp = std::chrono::system_clock::now();
                web_sample.label_confidence = kp.confidence;
                if (encoder_) {
                    web_sample.features = encoder_->encode(web_sample.text);
                }
                processSample(web_sample);
            }
        }

        // Throttle: max 0.5 samples/sec
        std::this_thread::sleep_for(std::chrono::seconds(2));
        yuki::infra::ModuleRegistry::instance().heartbeat("BackgroundLearningEngine");
    }
}

void BackgroundLearningEngine::processSample(const LearningSample& sample) {
    ++sample_count_;

    // Every 10th sample → inject synthetic VSE observation to keep inference warm
    if (++synthetic_counter_ >= 10) {
        synthetic_counter_ = 0;
        injectSyntheticVseObservation();
    }

    // Broadcast learning event to GW so CMF / NarrativeEngine can observe
    yuki::gw::Message msg;
    msg.topic         = yuki::gw::Topic::META_COGNITIVE;
    msg.source_module = "BackgroundLearningEngine";
    msg.salience      = 0.3f;
    msg.payload_json  = "{\"event\":\"learned\""
                        ",\"source\":\"" + sample.source + "\""
                        ",\"topic\":\""  + sample.topic  + "\""
                        ",\"text_len\":" + std::to_string(sample.text.length()) +
                        ",\"count\":"    + std::to_string(sample_count_.load()) + "}";
    yuki::gw::CoreBus::instance().publish(msg);
}

LearningSample BackgroundLearningEngine::generateCurriculumSample() {
    LearningSample sample;
    sample.source    = "curriculum";
    sample.timestamp = std::chrono::system_clock::now();

    if (!curriculum_topics_.empty()) {
        size_t idx  = curriculum_index_.fetch_add(1) % curriculum_topics_.size();
        sample.topic = curriculum_topics_[idx];
        sample.text  = "Curriculum review: " + sample.topic;
    } else {
        sample.topic = "general";
        sample.text  = "General knowledge consolidation";
    }

    if (encoder_) {
        sample.features = encoder_->encode(sample.text);
    }
    sample.label_confidence = 0.5f;
    return sample;
}

void BackgroundLearningEngine::injectSyntheticVseObservation() {
    if (!vse_) return;
    // Publish a low-salience synthetic observation so VSE doesn't stale
    yuki::gw::Message msg;
    msg.topic         = yuki::gw::Topic::META_COGNITIVE;
    msg.source_module = "BackgroundLearningEngine";
    msg.salience      = 0.1f;
    msg.payload_json  = "{\"event\":\"synthetic_vse_inject\",\"count\":"
                        + std::to_string(sample_count_.load()) + "}";
    yuki::gw::CoreBus::instance().publish(msg);
}
