#pragma once
// PhonologicalLoop — Verbal Working Memory / Inner Speech
// Implements Baddeley's phonological loop: phonological store + articulatory rehearsal
// Enables inner speech, subvocal rehearsal, and deliberation without motor output

#include <string>
#include <vector>
#include <deque>
#include <array>
#include <chrono>
#include <mutex>
#include "BrocaTranslator.h"
#include "WernickeComprehender.h"
#include "brain/memory/HdcSemanticGraph.h"

namespace yuki {
namespace brain {
namespace language {

using yuki::memory::HdcSemanticGraph;

struct PhonologicalConfig {
    size_t store_capacity = 7;  // Miller's 7±2
    std::chrono::milliseconds decay_time{2000};  // 2s decay without rehearsal
    std::chrono::milliseconds rehearsal_interval{500};  // Rehearsal every 500ms
    float rehearsal_strength = 0.9f;  // Strength per rehearsal
    int max_inner_speech_length = 128;  // Max tokens for inner speech
    bool enable_subvocal_rehearsal = true;
};

struct PhonologicalItem {
    std::string text;           // Phonological representation
    std::vector<float> embedding;  // Semantic embedding
    float activation = 1.0f;    // Current activation [0,1]
    std::chrono::steady_clock::time_point timestamp;
    bool is_rehearsed = false;
    int rehearsal_count = 0;
    std::string source;  // "perception", "inner_speech", "rehearsal"
};

struct PhonologicalLoopState {
    std::deque<PhonologicalItem> store;  // Phonological store
    std::string current_inner_speech;    // Current inner speech buffer
    std::vector<std::string> rehearsal_buffer;  // Items being rehearsed
    size_t total_items_processed = 0;
    float mean_activation = 0.0f;
};

class PhonologicalLoop {
public:
    explicit PhonologicalLoop(
        BrocaTranslator* broca = nullptr,
        WernickeComprehender* wernicke = nullptr,
        HdcSemanticGraph* hdc = nullptr);
    ~PhonologicalLoop();
    
    void setBrocaTranslator(BrocaTranslator* broca) noexcept { broca_ = broca; }
    void setWernickeComprehender(WernickeComprehender* wernicke) noexcept { wernicke_ = wernicke; }
    void setHdcSemanticGraph(HdcSemanticGraph* hdc) noexcept { hdc_ = hdc; }
    void setConfig(const PhonologicalConfig& cfg) { config_ = cfg; }
    
    // Start/stop the rehearsal loop
    void start();
    void stop();
    
    // Primary interface: perceive speech/text (enters phonological store)
    void perceive(const std::string& text, const std::vector<float>& embedding = {}, const std::string& source = "perception");
    
    // Generate inner speech (Broca → phonological store, no motor output)
    void innerSpeak(const std::string& prompt, const std::string& context = "");
    
    // Deliberate: maintain item in working memory via rehearsal
    void rehearse(const std::string& item);
    
    // Query current state
    PhonologicalLoopState getState() const;
    std::vector<std::string> getStoreContents() const;
    std::string getInnerSpeech() const;
    
    // Clear the store
    void clear();
    
    // Get most activated item (focus of attention)
    std::optional<PhonologicalItem> getFocus() const;
    
    // Articulatory rehearsal process (called by background thread)
    void rehearsalCycle();

private:
    BrocaTranslator* broca_ = nullptr;
    WernickeComprehender* wernicke_ = nullptr;
    HdcSemanticGraph* hdc_ = nullptr;
    PhonologicalConfig config_;
    
    mutable std::mutex mutex_;
    std::deque<PhonologicalItem> store_;
    std::string inner_speech_buffer_;
    std::vector<std::string> rehearsal_buffer_;
    std::atomic<bool> running_{false};
    std::thread rehearsal_thread_;
    std::chrono::steady_clock::time_point last_rehearsal_;
    
    // Decay activations based on time
    void decayActivations();
    
    // Add item to store with capacity management
    void addToStore(const PhonologicalItem& item);
    
    // Subvocal rehearsal: reactivate items
    void subvocalRehearsal();
    
    // Bind phonological form to semantic representation in HDC
    void bindToHdc(const PhonologicalItem& item);
};

} // namespace language
} // namespace brain
} // namespace yuki