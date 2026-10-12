#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <optional>
#include <filesystem>

#include "brain/language/GenerationBackend.h"

namespace yuki {
namespace brain {
namespace language {

using GenerationResult = yuki::brain::language::GenerationResult;

struct GenerationConfig {
    float temperature = 0.7f;
    int max_tokens = 512;
    float top_p = 0.9f;
    int top_k = 40;
    float repetition_penalty = 1.1f;
    std::vector<std::string> stop_sequences;
};

struct TokenizationResult {
    std::vector<int32_t> tokens;
    bool success = true;
    std::string error;
};

class LocalTransformer : public IGenerationBackend {
public:
    using GenerationResult = yuki::brain::language::GenerationResult;
    
    explicit LocalTransformer();
    ~LocalTransformer() override;

    bool loadModel(const std::filesystem::path& model_path);
    bool reloadModel(const std::filesystem::path& model_path);
    bool isLoaded() const { return loaded_; }
    bool available() const override { return loaded_; }
    BackendKind kind() const override { return BackendKind::LOCAL_TRANSFORMER_CPU; }
    std::string name() const override { return "LocalTransformer"; }
    float estimateCost(const GenerationRequest&) const override { return 0.0f; }

    const std::filesystem::path& modelPath() const { return model_path_; }

    TokenizationResult tokenize(const std::string& text, bool add_special = true) const;
    std::string detokenize(const std::vector<int32_t>& tokens) const;

    GenerationResult generate(const GenerationRequest& request) override;
    GenerationResult generate(const std::string& prompt, const GenerationConfig& config = {});
    GenerationResult generateWithContext(const std::vector<int32_t>& system_tokens,
                                         const std::vector<int32_t>& user_tokens,
                                         const GenerationConfig& config = {});

    int32_t bosToken() const { return bos_token_; }
    int32_t eosToken() const { return eos_token_; }
    int32_t vocabSize() const { return vocab_size_; }
    int maxContext() const { return max_context_; }
    static constexpr int EMBED_DIM = 2048;

    struct EmbeddingResult {
        std::vector<float> embedding;
        bool success = true;
        std::string error;
    };
    EmbeddingResult getEmbedding(const std::string& text) const;
    std::vector<float> embed(const std::string& text) const { return getEmbedding(text).embedding; }

    bool loadLoraAdapter(const std::string& adapter_path, float scale = 1.0f);
    bool unloadLoraAdapter();
    std::string getActiveLoraAdapterPath() const { return active_lora_adapter_path_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Serializes model use against a hot-reload. loadModel() frees the live
    // context/model, so a reload racing an in-flight generate()/embed() would
    // use freed weights. Recursive because the generated entry points call one
    // another (generate -> tokenize/generateWithContext).
    mutable std::recursive_mutex modelMutex_;

    bool loaded_ = false;
    std::filesystem::path model_path_;
    std::string active_lora_adapter_path_;
    float active_lora_scale_ = 1.0f;
    int32_t bos_token_ = 0;
    int32_t eos_token_ = 0;
    int32_t vocab_size_ = 0;
    int max_context_ = 2048;
};

} // namespace language
} // namespace brain
} // namespace yuki