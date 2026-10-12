#include "brain/language/LocalTransformer.h"
#include "brain/core/Logger.h"
#include <llama.h>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <thread>

namespace yuki {
namespace brain {
namespace language {

struct LocalTransformer::Impl {
    llama_model* model = nullptr;
    llama_context* context = nullptr;
    const struct llama_vocab* vocab = nullptr;
    struct llama_adapter_lora* active_adapter = nullptr;
    std::vector<int32_t> token_cache;
};

LocalTransformer::LocalTransformer() : impl_(std::make_unique<Impl>()) {}

LocalTransformer::~LocalTransformer() {
    std::lock_guard<std::recursive_mutex> lock(modelMutex_);
    if (impl_->active_adapter) {
        llama_adapter_lora_free(impl_->active_adapter);
        impl_->active_adapter = nullptr;
    }
    if (impl_->context) {
        llama_free(impl_->context);
    }
    if (impl_->model) {
        llama_model_free(impl_->model);
    }
    llama_backend_free();
}

bool LocalTransformer::loadModel(const std::filesystem::path& model_path) {
    std::lock_guard<std::recursive_mutex> lock(modelMutex_);
    if (loaded_) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "LocalTransformer",
            "Model already loaded, unloading first");
        // llama.cpp has no in-place weight swap: release the live model and
        // context before loading the replacement. Previously this branch only
        // logged, leaking the previous model on every hot-reload.
        if (impl_->context) { llama_free(impl_->context);         impl_->context = nullptr; }
        if (impl_->model)   { llama_model_free(impl_->model);     impl_->model = nullptr;   }
        impl_->vocab = nullptr;
        loaded_ = false;
    }

    llama_backend_init();
    
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = -1;  // Use all GPU layers

    impl_->model = llama_model_load_from_file(model_path.string().c_str(), model_params);
    if (!impl_->model) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "LocalTransformer",
            "Failed to load model: " + model_path.string());
        return false;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 4096;
    ctx_params.n_batch = 512;
    ctx_params.n_threads = std::thread::hardware_concurrency();
    ctx_params.n_threads_batch = std::thread::hardware_concurrency();

    impl_->context = llama_init_from_model(impl_->model, ctx_params);
    if (!impl_->context) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "LocalTransformer",
            "Failed to create context");
        llama_model_free(impl_->model);
        impl_->model = nullptr;
        return false;
    }

    impl_->vocab = llama_model_get_vocab(impl_->model);
    if (!impl_->vocab) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "LocalTransformer",
            "Failed to get vocabulary");
        return false;
    }

    model_path_ = std::filesystem::absolute(model_path);
    vocab_size_ = llama_vocab_n_tokens(impl_->vocab);
    bos_token_ = llama_vocab_bos(impl_->vocab);
    eos_token_ = llama_vocab_eos(impl_->vocab);
    max_context_ = llama_n_ctx(impl_->context);
    loaded_ = true;

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "LocalTransformer",
        "Model loaded: " + model_path_.string() + " (vocab=" + std::to_string(vocab_size_) + ")");
    return true;
}

bool LocalTransformer::reloadModel(const std::filesystem::path& model_path) {
    std::lock_guard<std::recursive_mutex> lock(modelMutex_);
    if (!std::filesystem::exists(model_path)) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "LocalTransformer",
            "reloadModel: model file not found: " + model_path.string());
        return false;
    }
    // loadModel() releases the previous weights itself, so a reload is a
    // validated re-load (used by the post-distillation hot-reload path).
    return loadModel(model_path);
}

TokenizationResult LocalTransformer::tokenize(const std::string& text, bool add_special) const {
    std::lock_guard<std::recursive_mutex> lock(modelMutex_);
    TokenizationResult result;
    if (!loaded_) {
        result.success = false;
        result.error = "Model not loaded";
        return result;
    }

    int n_tokens = -llama_tokenize(impl_->vocab, text.c_str(), static_cast<int32_t>(text.size()), nullptr, 0, add_special, true);
    if (n_tokens < 0) {
        result.success = false;
        result.error = "Tokenization failed";
        return result;
    }

    impl_->token_cache.resize(n_tokens);
    n_tokens = llama_tokenize(impl_->vocab, text.c_str(), static_cast<int32_t>(text.size()), impl_->token_cache.data(), n_tokens, add_special, true);
    if (n_tokens < 0) {
        result.success = false;
        result.error = "Tokenization failed (2nd pass)";
        return result;
    }

    result.tokens.resize(n_tokens);
    for (int i = 0; i < n_tokens; ++i) {
        result.tokens[i] = static_cast<int32_t>(impl_->token_cache[i]);
    }
    result.success = true;
    return result;
}

std::string LocalTransformer::detokenize(const std::vector<int32_t>& tokens) const {
    std::lock_guard<std::recursive_mutex> lock(modelMutex_);
    if (!loaded_) return "";
    if (tokens.empty()) return "";

    std::string result;
    result.reserve(tokens.size() * 4);
    for (int32_t token : tokens) {
        char buf[64];
        int n = llama_token_to_piece(impl_->vocab, static_cast<llama_token>(token), buf, sizeof(buf), 0, true);
        if (n > 0) result.append(buf, n);
    }
    return result;
}

LocalTransformer::GenerationResult LocalTransformer::generate(const GenerationRequest& request) {
    GenerationConfig cfg;
    cfg.temperature = request.temperature;
    cfg.max_tokens = request.maxTokens;
    cfg.top_p = request.topP;
    auto res = generate(request.prompt, cfg);
    res.backend = BackendKind::LOCAL_TRANSFORMER_CPU;
    res.backendName = "LocalTransformer";
    return res;
}

LocalTransformer::GenerationResult LocalTransformer::generate(const std::string& prompt, const GenerationConfig& config) {
    GenerationResult result;
    result.backend = BackendKind::LOCAL_TRANSFORMER_CPU;
    result.backendName = "LocalTransformer";
    if (!loaded_) {
        result.success = false;
        result.failureReason = "Model not loaded";
        return result;
    }

    auto tok_result = tokenize(prompt, true);
    if (!tok_result.success) {
        result.success = false;
        result.failureReason = tok_result.error;
        return result;
    }

    return generateWithContext({}, tok_result.tokens, config);
}

LocalTransformer::GenerationResult LocalTransformer::generateWithContext(
    const std::vector<int32_t>& system_tokens,
    const std::vector<int32_t>& user_tokens,
    const GenerationConfig& config) {
    std::lock_guard<std::recursive_mutex> lock(modelMutex_);

    GenerationResult result;
    result.backend = BackendKind::LOCAL_TRANSFORMER_CPU;
    result.backendName = "LocalTransformer";
    if (!loaded_) {
        result.success = false;
        result.failureReason = "Model not loaded";
        return result;
    }

    std::vector<llama_token> tokens;
    tokens.reserve(system_tokens.size() + user_tokens.size() + config.max_tokens);

    for (int32_t t : system_tokens) tokens.push_back(static_cast<llama_token>(t));
    for (int32_t t : user_tokens) tokens.push_back(static_cast<llama_token>(t));

    auto start = std::chrono::high_resolution_clock::now();
    std::string generated_text;
    generated_text.reserve(config.max_tokens * 4);

    llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(config.top_k));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(config.top_p, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(config.temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_penalties(vocab_size_, 64, config.repetition_penalty, 0.0f, 0.0f));

    // Decode the full prompt first — sampling requires logits from the last
    // prompt token. Sampling before any decode hits "corrupt output buffer"
    // (n_outputs=0) and GGML_ASSERT(logits != nullptr).
    if (!tokens.empty()) {
        llama_batch prompt_batch = llama_batch_get_one(tokens.data(), static_cast<int32_t>(tokens.size()));
        if (llama_decode(impl_->context, prompt_batch) != 0) {
            llama_sampler_free(sampler);
            result.success = false;
            result.failureReason = "Prompt decode failed";
            return result;
        }
    }

    for (int i = 0; i < config.max_tokens; ++i) {
        llama_token token = llama_sampler_sample(sampler, impl_->context, -1);
        if (token == eos_token_) break;

        std::string piece = detokenize({static_cast<int32_t>(token)});
        if (!piece.empty() && piece != "<|endoftext|>") {
            generated_text += piece;
        }

        if (llama_decode(impl_->context, llama_batch_get_one(&token, 1)) != 0) {
            break;
        }
        tokens.push_back(token);
    }

    llama_sampler_free(sampler);
    auto end = std::chrono::high_resolution_clock::now();
    result.latencyMs = std::chrono::duration<float, std::milli>(end - start).count();
    result.elapsedMs = result.latencyMs;
    result.outputTokenCount = static_cast<int>(generated_text.size() / 4);
    result.text = generated_text;
    result.success = true;
    return result;
}

LocalTransformer::EmbeddingResult LocalTransformer::getEmbedding(const std::string& text) const {
    std::lock_guard<std::recursive_mutex> lock(modelMutex_);
    EmbeddingResult result;
    if (!loaded_) {
        result.success = false;
        result.error = "Model not loaded";
        return result;
    }

    auto tok_result = tokenize(text, true);
    if (!tok_result.success) {
        result.success = false;
        result.error = tok_result.error;
        return result;
    }

    llama_batch batch = llama_batch_get_one(tok_result.tokens.data(), tok_result.tokens.size());
    if (llama_decode(impl_->context, batch) != 0) {
        result.success = false;
        result.error = "Decode failed";
        return result;
    }

    const float* embeddings = llama_get_embeddings_seq(impl_->context, 0);
    int n_embd = llama_model_n_embd(impl_->model);
    result.embedding.assign(embeddings, embeddings + n_embd);
    result.success = true;
    return result;
}

bool LocalTransformer::loadLoraAdapter(const std::string& adapter_path, float scale) {
    std::lock_guard<std::recursive_mutex> lock(modelMutex_);
    if (!loaded_ || !impl_->context || !impl_->model) return false;
    if (!std::filesystem::exists(adapter_path)) return false;

    if (impl_->active_adapter) {
        llama_adapter_lora_free(impl_->active_adapter);
        impl_->active_adapter = nullptr;
    }

    impl_->active_adapter = llama_adapter_lora_init(impl_->model, adapter_path.c_str());
    if (!impl_->active_adapter) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "LocalTransformer",
            "Failed to initialize LoRA adapter: " + adapter_path);
        return false;
    }

    struct llama_adapter_lora* adapters[1] = { impl_->active_adapter };
    float scales[1] = { scale };
    llama_set_adapters_lora(impl_->context, adapters, 1, scales);

    active_lora_adapter_path_ = adapter_path;
    active_lora_scale_ = scale;
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "LocalTransformer",
        "Loaded LoRA adapter: " + adapter_path + " (scale=" + std::to_string(scale) + ")");
    return true;
}

bool LocalTransformer::unloadLoraAdapter() {
    std::lock_guard<std::recursive_mutex> lock(modelMutex_);
    if (!loaded_ || !impl_->context) return false;
    if (impl_->active_adapter) {
        llama_set_adapters_lora(impl_->context, nullptr, 0, nullptr);
        llama_adapter_lora_free(impl_->active_adapter);
        impl_->active_adapter = nullptr;
    }
active_lora_adapter_path_.clear();
    active_lora_scale_ = 1.0f;
    return true;
}

} // namespace language
} // namespace brain
} // namespace yuki