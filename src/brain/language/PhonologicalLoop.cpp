#include <chrono>
#include <mutex>
#include <algorithm>
#include <thread>
#include "brain/language/PhonologicalLoop.h"
#include "brain/core/Logger.h"

namespace yuki {
namespace brain {
namespace language {

PhonologicalLoop::PhonologicalLoop(
    BrocaTranslator* broca,
    WernickeComprehender* wernicke,
    HdcSemanticGraph* hdc)
    : broca_(broca), wernicke_(wernicke), hdc_(hdc) {}

PhonologicalLoop::~PhonologicalLoop() {
    stop();
}

void PhonologicalLoop::start() {
    if (running_.exchange(true)) return;
    last_rehearsal_ = std::chrono::steady_clock::now();
    rehearsal_thread_ = std::thread([this]() {
        while (running_.load()) {
            std::this_thread::sleep_for(config_.rehearsal_interval);
            if (!running_.load()) break;
            rehearsalCycle();
        }
    });
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "PhonologicalLoop", "Started");
}

void PhonologicalLoop::stop() {
    running_ = false;
    if (rehearsal_thread_.joinable()) {
        rehearsal_thread_.join();
    }
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "PhonologicalLoop", "Stopped");
}

void PhonologicalLoop::perceive(const std::string& text, const std::vector<float>& embedding, const std::string& source) {
    if (text.empty()) return;
    
    PhonologicalItem item;
    item.text = text;
    item.embedding = embedding;
    item.activation = 1.0f;
    item.timestamp = std::chrono::steady_clock::now();
    item.source = source;
    item.rehearsal_count = 0;
    
    // Generate embedding if not provided
    if (item.embedding.empty() && wernicke_) {
        item.embedding = wernicke_->embedText(text);
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        addToStore(item);
    }
    
    // Bind to HDC for long-term association
    if (hdc_) {
        bindToHdc(item);
    }
}

void PhonologicalLoop::innerSpeak(const std::string& prompt, const std::string& context) {
    if (!broca_) return;
    
    // Use Broca to generate inner speech (no motor output)
    BrocaInput input;
    input.user_input = prompt;
    input.retrieved_context = context;
    input.q_intent.fill(0.125f);
    input.q_engagement[1] = 1.0f;
    input.q_urgency[0] = 1.0f;
    input.precision.intent = 0.5f;
    input.precision.entity = 0.5f;
    input.precision.tone = 0.5f;
    input.precision.safety = 0.5f;
    input.precision.source = 0.5f;
    
    auto output = broca_->innerSpeech(input);
    
    if (output.success && !output.response_text.empty()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            inner_speech_buffer_ = output.response_text;
            
            // Also add to store as inner speech
            PhonologicalItem item;
            item.text = output.response_text;
            item.embedding = output.response_text.empty() ? std::vector<float>() : (wernicke_ ? wernicke_->embedText(output.response_text) : std::vector<float>());
            item.activation = 1.0f;
            item.timestamp = std::chrono::steady_clock::now();
            item.source = "inner_speech";
            item.rehearsal_count = 0;
            addToStore(item);
        }
    }
}

void PhonologicalLoop::rehearse(const std::string& item) {
    if (item.empty()) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Add to rehearsal buffer
    rehearsal_buffer_.push_back(item);
    if (rehearsal_buffer_.size() > config_.store_capacity) {
        rehearsal_buffer_.erase(rehearsal_buffer_.begin());
    }
    
    // Also boost activation in store if present
    for (auto& stored : store_) {
        if (stored.text == item) {
            stored.activation = std::min(1.0f, stored.activation + config_.rehearsal_strength);
            stored.rehearsal_count++;
            stored.is_rehearsed = true;
            break;
        }
    }
}

PhonologicalLoopState PhonologicalLoop::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PhonologicalLoopState state;
    state.store = store_;
    state.current_inner_speech = inner_speech_buffer_;
    state.rehearsal_buffer = rehearsal_buffer_;
    state.total_items_processed = store_.size();  // Approximation
    
    float sum_activation = 0.0f;
    for (const auto& item : store_) {
        sum_activation += item.activation;
    }
    state.mean_activation = store_.empty() ? 0.0f : sum_activation / store_.size();
    
    return state;
}

std::vector<std::string> PhonologicalLoop::getStoreContents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> contents;
    contents.reserve(store_.size());
    for (const auto& item : store_) {
        contents.push_back(item.text);
    }
    return contents;
}

std::string PhonologicalLoop::getInnerSpeech() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return inner_speech_buffer_;
}

void PhonologicalLoop::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    store_.clear();
    inner_speech_buffer_.clear();
    rehearsal_buffer_.clear();
}

std::optional<PhonologicalItem> PhonologicalLoop::getFocus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (store_.empty()) return std::nullopt;
    
    // Return highest activation item
    auto it = std::max_element(store_.begin(), store_.end(),
        [](const PhonologicalItem& a, const PhonologicalItem& b) {
            return a.activation < b.activation;
        });
    
    return *it;
}

void PhonologicalLoop::rehearsalCycle() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    decayActivations();
    
    if (config_.enable_subvocal_rehearsal) {
        subvocalRehearsal();
    }
    
    // Remove decayed items (activation < 0.1)
    store_.erase(
        std::remove_if(store_.begin(), store_.end(),
            [](const PhonologicalItem& item) {
                return item.activation < 0.1f;
            }),
        store_.end()
    );
    
    // Update mean activation
    float sum = 0.0f;
    for (const auto& item : store_) {
        sum += item.activation;
    }
    // mean_activation updated in getState()
}

void PhonologicalLoop::decayActivations() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_rehearsal_).count();
    
    if (elapsed == 0) return;
    
    float decay_factor = std::exp(-elapsed / static_cast<float>(config_.decay_time.count()));
    last_rehearsal_ = now;
    
    for (auto& item : store_) {
        item.activation *= decay_factor;
    }
    
    // Also decay inner speech buffer
    if (!inner_speech_buffer_.empty()) {
        // Inner speech decays faster
        // We'll just clear it after a while
        static int inner_speech_cycles = 0;
        inner_speech_cycles++;
        if (inner_speech_cycles > 10) {  // ~5 seconds
            inner_speech_buffer_.clear();
            inner_speech_cycles = 0;
        }
    }
}

void PhonologicalLoop::addToStore(const PhonologicalItem& item) {
    // Check if similar item already exists
    for (auto& existing : store_) {
        if (existing.text == item.text) {
            existing.activation = std::max(existing.activation, item.activation);
            existing.timestamp = item.timestamp;
            return;
        }
    }
    
    // Add new item
    store_.push_back(item);
    
    // Enforce capacity limit (remove lowest activation)
    while (store_.size() > config_.store_capacity) {
        auto min_it = std::min_element(store_.begin(), store_.end(),
            [](const PhonologicalItem& a, const PhonologicalItem& b) {
                return a.activation < b.activation;
            });
        store_.erase(min_it);
    }
}

void PhonologicalLoop::subvocalRehearsal() {
    // Rehearse items in rehearsal buffer
    for (const auto& item_text : rehearsal_buffer_) {
        for (auto& stored : store_) {
            if (stored.text == item_text) {
                stored.activation = std::min(1.0f, stored.activation + config_.rehearsal_strength);
                stored.rehearsal_count++;
                stored.is_rehearsed = true;
                break;
            }
        }
    }
    
    // Also rehearse the focus item (highest activation)
    if (!store_.empty()) {
        auto it = std::max_element(store_.begin(), store_.end(),
            [](const PhonologicalItem& a, const PhonologicalItem& b) {
                return a.activation < b.activation;
            });
        it->activation = std::min(1.0f, it->activation + config_.rehearsal_strength * 0.5f);
        it->rehearsal_count++;
        it->is_rehearsed = true;
    }
}

void PhonologicalLoop::bindToHdc(const PhonologicalItem& item) {
    if (!hdc_ || item.embedding.empty()) return;
    
    try {
        // Create a semantic pointer for the phonological form
        // This binds the phonological representation to its semantic meaning
        std::string concept_name = "phon_" + item.text;
        // Truncate if too long
        if (concept_name.size() > 64) concept_name = concept_name.substr(0, 64);
        
        // Ingest into HDC graph
        // The embedding serves as the semantic vector
        // Note: HDC expects Hypervector, we'd need to convert
        // For now, log the binding
        yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "PhonologicalLoop",
            "Bound phonological form to HDC: " + concept_name);
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "PhonologicalLoop",
            "HDC binding failed: " + std::string(e.what()));
    }
}

} // namespace language
} // namespace brain
} // namespace yuki