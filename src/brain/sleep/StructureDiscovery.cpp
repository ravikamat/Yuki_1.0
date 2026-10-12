#include "brain/sleep/StructureDiscovery.h"
#include "brain/core/Logger.h"
#include "brain/memory/EpisodicStore.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <random>
#include <cmath>
#include <queue>
#include <filesystem>

namespace yuki {
namespace brain {
namespace sleep {

using simulation::BranchManagement;

StructureDiscovery::StructureDiscovery(const StructureDiscoveryConfig& cfg)
    : config_(cfg), last_discovery_(std::chrono::steady_clock::now()) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "StructureDiscovery",
        "Initialized with export_dir=" + config_.export_directory);
    
    // Ensure export directory exists
    std::filesystem::create_directories(config_.export_directory);
}

size_t StructureDiscovery::runDiscoveryEpoch(float& avg_confidence_out) {
    if (!episodic_store_ || !causal_world_model_) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "StructureDiscovery",
            "Missing dependencies, skipping discovery epoch");
        avg_confidence_out = 0.0f;
        return 0;
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "StructureDiscovery",
        "Starting structure discovery epoch");

    // Learn causal structure
    DiscoveredStructure structure = learnCausalStructure();
    
    if (structure.nodes.empty()) {
        avg_confidence_out = 0.0f;
        return 0;
    }

    // Validate structure
    float validation_score = validateStructure(structure);
    
    // Discover edges from surprise
    auto surprise_edges = discoverEdgesFromSurprise();
    structure.edges.insert(structure.edges.end(), surprise_edges.begin(), surprise_edges.end());

    // Update stats
    stats_.total_discoveries++;
    stats_.avg_structure_confidence = 
        (stats_.avg_structure_confidence * (stats_.total_discoveries - 1) + structure.overall_confidence) 
        / stats_.total_discoveries;

    if (validation_score > config_.edge_confidence_threshold) {
        stats_.successful_validations++;
    }

    avg_confidence_out = structure.overall_confidence;
    last_discovery_ = std::chrono::steady_clock::now();

    // Export if confidence is high enough
    if (structure.overall_confidence > config_.edge_confidence_threshold) {
        exportDistillation();
    }

    return structure.edges.size();
}

void StructureDiscovery::discoverStructures() {
    float avg_conf;
    runDiscoveryEpoch(avg_conf);
}

DiscoveredStructure StructureDiscovery::learnCausalStructure() {
    DiscoveredStructure structure;
    
    // Extract transition data
    TransitionData data = extractTransitions(config_.max_nodes);
    
    if (data.states.empty()) {
        return structure;
    }

    // Run PC algorithm
    structure = runPCAlgorithm(data);

    // Compute centrality for nodes
    for (auto& node : structure.nodes) {
        node.centrality = computeCentrality(node.id, structure);
    }

    // Compute overall confidence
    float total_conf = 0.0f;
    for (const auto& edge : structure.edges) {
        total_conf += edge.confidence;
    }
    structure.overall_confidence = structure.edges.empty() ? 0.0f : total_conf / structure.edges.size();
    structure.discovered_at = std::chrono::steady_clock::now();
    structure.source_episodes = data.episode_ids.size();

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "StructureDiscovery",
        "Discovered " + std::to_string(structure.nodes.size()) + " nodes, " +
        std::to_string(structure.edges.size()) + " edges, confidence=" + 
        std::to_string(structure.overall_confidence));

    return structure;
}

std::vector<DiscoveredStructure::Edge> StructureDiscovery::discoverEdgesFromSurprise() {
    std::vector<DiscoveredStructure::Edge> edges;

    if (!episodic_store_ || !causal_world_model_) return edges;

    // Get high-surprise episodes
    auto transitions = extractTransitions(config_.max_nodes);
    
    // Find episodes with high surprise
    for (size_t i = 0; i < transitions.surprises.size(); ++i) {
        if (transitions.surprises[i] > config_.min_surprise_for_discovery) {
            // Create edge from surprise context
            DiscoveredStructure::Edge edge;
            edge.from = static_cast<uint32_t>(i % config_.max_nodes);
            edge.to = static_cast<uint32_t>((i + 1) % config_.max_nodes);
            edge.confidence = transitions.surprises[i];
            edge.strength = transitions.rewards.empty() ? 0.5f : transitions.rewards[i];
            edge.relation_type = "surprise_induced";
            edge.evidence_count = 1;
            edge.supporting_episodes.push_back(transitions.episode_ids[i]);
            edges.push_back(edge);
        }
    }

    // Also check contradiction detector
    if (contradiction_detector_) {
        auto contradictions = contradiction_detector_->detectCausalContradictions();
        for (const auto& c : contradictions) {
            DiscoveredStructure::Edge edge;
            edge.from = 0;  // Would map from contradiction entities
            edge.to = 1;
            edge.confidence = c.confidence;
            edge.strength = c.severity;
            edge.relation_type = "contradiction_resolved";
            edges.push_back(edge);
        }
    }

    return edges;
}

float StructureDiscovery::validateStructure(const DiscoveredStructure& structure) const {
    if (!causal_world_model_ || structure.edges.empty()) return 0.0f;

    float valid_count = 0.0f;
    for (const auto& edge : structure.edges) {
        if (validateEdge(edge, extractTransitions(1000))) {
            valid_count += 1.0f;
        }
    }

    return structure.edges.empty() ? 0.0f : valid_count / structure.edges.size();
}

bool StructureDiscovery::validateEdge(const DiscoveredStructure::Edge& edge, 
                                      const TransitionData& data) const {
    if (data.states.size() < 10) return false;

    // Simple validation: check if edge.from state predicts edge.to state
    size_t from_idx = edge.from % data.states.size();
    size_t to_idx = edge.to % data.states.size();

    if (from_idx >= data.states.size() || to_idx >= data.states.size()) return false;

    // Compute correlation
    float corr = 0.0f;
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f, sum_y2 = 0.0f;
    size_t n = std::min(data.states[from_idx].size(), data.states[to_idx].size());
    n = std::min(n, size_t(100));  // Limit for speed

    for (size_t i = 0; i < n; ++i) {
        float x = data.states[from_idx][i];
        float y = data.states[to_idx][i];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
        sum_y2 += y * y;
    }

    float denom = std::sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));
    if (denom > 0) {
        corr = (n * sum_xy - sum_x * sum_y) / denom;
    }

    return std::abs(corr) > 0.3f;  // Minimum correlation threshold
}

float StructureDiscovery::computeCentrality(uint32_t node_id, const DiscoveredStructure& structure) const {
    float centrality = 0.0f;
    for (const auto& edge : structure.edges) {
        if (edge.from == node_id || edge.to == node_id) {
            centrality += edge.confidence * edge.strength;
        }
    }
    return centrality;
}

StructureDiscovery::TransitionData StructureDiscovery::extractTransitions(size_t max_episodes) const {
    TransitionData data;
    
    if (!episodic_store_) return data;

    // This is a simplified extraction - in production would query actual episodic store
    // For now, generate synthetic data from causal world model if available
    if (causal_world_model_) {
        auto transitions = causal_world_model_->getTransitions();
        for (const auto& [key, trans] : transitions) {
            if (data.states.size() >= max_episodes) break;
            
            // Extract state representations from transition
            data.states.push_back(trans.W);  // Simplified
            data.actions.push_back({});       // Would extract actual actions
            data.next_states.push_back({});
            data.rewards.push_back(trans.running_loss > 0 ? -trans.running_loss : 0.5f);
            data.surprises.push_back(std::min(1.0f, trans.confidence));
            data.episode_ids.push_back(trans.observation_count);
        }
    }

    return data;
}

DiscoveredStructure StructureDiscovery::runPCAlgorithm(const TransitionData& data) {
    DiscoveredStructure structure;
    
    if (data.states.empty()) return structure;

    size_t n_vars = std::min(data.states.size(), static_cast<size_t>(config_.max_nodes));
    
    // Create nodes
    for (size_t i = 0; i < n_vars; ++i) {
        DiscoveredStructure::Node node;
        node.id = next_node_id_++;
        node.label = "Variable_" + std::to_string(i);
        node.concept_type = "latent";
        structure.nodes.push_back(node);
    }

    // Simple correlation-based edge discovery (PC algorithm approximation)
    // In production, would use full PC algorithm with conditional independence tests
    for (size_t i = 0; i < n_vars; ++i) {
        for (size_t j = i + 1; j < n_vars; ++j) {
            // Compute correlation
            float corr = 0.0f;
            size_t n = std::min(data.states[i].size(), data.states[j].size());
            n = std::min(n, size_t(100));
            
            if (n < 5) continue;
            
            float sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
            for (size_t k = 0; k < n; ++k) {
                float x = data.states[i][k];
                float y = data.states[j][k];
                sum_x += x;
                sum_y += y;
                sum_xy += x * y;
                sum_x2 += x * x;
                sum_y2 += y * y;
            }
            
            float denom = std::sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));
            if (denom > 0) {
                corr = (n * sum_xy - sum_x * sum_y) / denom;
            }
            
            if (std::abs(corr) > config_.independence_threshold) {
                DiscoveredStructure::Edge edge;
                edge.from = structure.nodes[i].id;
                edge.to = structure.nodes[j].id;
                edge.confidence = std::abs(corr);
                edge.strength = corr;
                edge.relation_type = (corr > 0) ? "positive_correlation" : "negative_correlation";
                edge.evidence_count = static_cast<int>(n);
                structure.edges.push_back(edge);
            }
        }
    }

    // Prune low-confidence edges
    structure.edges.erase(
        std::remove_if(structure.edges.begin(), structure.edges.end(),
            [this](const DiscoveredStructure::Edge& e) {
                return e.confidence < config_.prune_threshold;
            }),
        structure.edges.end());

    return structure;
}

DistillationExport StructureDiscovery::exportDistillation() {
    DistillationExport export_result;
    export_result.exported_at = std::chrono::steady_clock::now();
    
    try {
        // Learn structure if not recently done
        DiscoveredStructure structure = learnCausalStructure();
        
        if (structure.edges.empty()) {
            export_result.success = false;
            export_result.error_message = "No structure discovered";
            return export_result;
        }

        // Generate timestamp for filenames
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
        std::string timestamp = ss.str();

        // Export structure
        if (config_.export_structure) {
            export_result.structure_path = config_.export_directory + "/structure_" + timestamp + ".json";
            exportStructureJson(export_result.structure_path, structure);
            export_result.structures = 1;
        }

        // Generate and export SFT samples
        if (config_.export_sft) {
            export_result.sft_path = config_.export_directory + "/sft_" + timestamp + ".jsonl";
            auto sft_samples = generateSFTFromStructure(structure);
            export_result.sft_samples = writeSFTJsonl(export_result.sft_path, sft_samples);
            stats_.total_sft_exported += export_result.sft_samples;
        }

        // Generate and export DPO pairs
        if (config_.export_dpo) {
            export_result.dpo_path = config_.export_directory + "/dpo_" + timestamp + ".jsonl";
            auto dpo_pairs = generateDPOFromStructure(structure);
            export_result.dpo_pairs = writeDPOJsonl(export_result.dpo_path, dpo_pairs);
            stats_.total_dpo_exported += export_result.dpo_pairs;
        }

        export_result.success = true;
        stats_.total_exports++;

        yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "StructureDiscovery",
            "Exported distillation: " + std::to_string(export_result.sft_samples) + " SFT, " +
            std::to_string(export_result.dpo_pairs) + " DPO");

    } catch (const std::exception& e) {
        export_result.success = false;
        export_result.error_message = e.what();
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "StructureDiscovery",
            "Export failed: " + std::string(e.what()));
    }

    return export_result;
}

bool StructureDiscovery::exportStructureJson(const std::string& path, 
                                              const DiscoveredStructure& structure) const {
    std::ofstream out(path);
    if (!out) return false;

    out << "{\n";
    out << "  \"discovered_at\": \"" << 
        std::chrono::duration_cast<std::chrono::seconds>(
            structure.discovered_at.time_since_epoch()).count() << "\",\n";
    out << "  \"overall_confidence\": " << structure.overall_confidence << ",\n";
    out << "  \"source_episodes\": " << structure.source_episodes << ",\n";
    out << "  \"nodes\": [\n";
    
    for (size_t i = 0; i < structure.nodes.size(); ++i) {
        const auto& n = structure.nodes[i];
        out << "    {\"id\": " << n.id << ", \"label\": \"" << n.label 
            << "\", \"type\": \"" << n.concept_type 
            << "\", \"centrality\": " << n.centrality << "}";
        if (i + 1 < structure.nodes.size()) out << ",";
        out << "\n";
    }
    
    out << "  ],\n  \"edges\": [\n";
    
    for (size_t i = 0; i < structure.edges.size(); ++i) {
        const auto& e = structure.edges[i];
        out << "    {\"from\": " << e.from << ", \"to\": " << e.to
            << ", \"confidence\": " << e.confidence
            << ", \"strength\": " << e.strength
            << ", \"type\": \"" << e.relation_type << "\""
            << ", \"evidence\": " << e.evidence_count << "}";
        if (i + 1 < structure.edges.size()) out << ",";
        out << "\n";
    }
    
    out << "  ]\n}\n";
    return true;
}

std::vector<std::string> StructureDiscovery::exportCurriculumSeeds(const std::string& base_dir) const {
    std::vector<std::string> paths;
    std::filesystem::create_directories(base_dir);

    // Export curriculum seeds for different stages
    const std::vector<std::pair<std::string, std::string>> stages = {
        {"stage0_basic", "basic_concepts"},
        {"stage1_tool_use", "tool_use_patterns"},
        {"stage2_reasoning", "reasoning_chains"},
        {"stage3_social", "social_interactions"}
    };

    for (const auto& [stage_name, stage_desc] : stages) {
        std::string path = base_dir + "/" + stage_name + ".jsonl";
        // Generate simplified samples for each stage
        std::ofstream out(path);
        if (out) {
            out << "{\"stage\": \"" << stage_desc << "\", \"samples\": []}\n";
            paths.push_back(path);
        }
    }

    return paths;
}

std::vector<uint64_t> StructureDiscovery::generateHypothesesFromStructure(const DiscoveredStructure& structure) {
    std::vector<uint64_t> hypothesis_ids;
    
    if (!branch_management_) return hypothesis_ids;

    for (const auto& edge : structure.edges) {
        if (edge.confidence > config_.edge_confidence_threshold) {
            std::string hypothesis = "Variable " + std::to_string(edge.from) + 
                                   " " + edge.relation_type + " " + 
                                   std::to_string(edge.to);
            
            uint64_t hyp_id = branch_management_->proposeHypothesis(
                hypothesis,
                "edge(" + std::to_string(edge.from) + "," + std::to_string(edge.to) + ")",
                edge.confidence);
            hypothesis_ids.push_back(hyp_id);
        }
    }

    return hypothesis_ids;
}

void StructureDiscovery::testHypotheses(const std::vector<uint64_t>& hypothesis_ids) {
    if (!virtual_simulation_ || !branch_management_) return;

    for (uint64_t hyp_id : hypothesis_ids) {
        auto* hyp = branch_management_->getHypothesis(hyp_id);
        if (!hyp) continue;

        // Run simulation for this hypothesis
        if (hyp->simulation_branch_id != 0) {
            auto result = virtual_simulation_->runBranch(hyp->simulation_branch_id);
            
            // Add result as evidence
            yuki::brain::simulation::Evidence ev;
            ev.type = yuki::brain::simulation::Evidence::Type::SIMULATION;
            ev.description = "Structure test: " + result.summary;
            ev.weight = result.confidence;
            ev.reliability = 0.8f;
            ev.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            ev.hypothesis_id = hyp_id;
            ev.source = "StructureDiscovery";
            branch_management_->addEvidence(hyp_id, ev);
        }
    }
}

size_t StructureDiscovery::writeSFTJsonl(const std::string& path, 
                                          const std::vector<std::string>& samples) const {
    std::ofstream out(path);
    if (!out) return 0;

    size_t count = 0;
    for (const auto& sample : samples) {
        if (count >= config_.max_sft_samples) break;
        out << sample << "\n";
        count++;
    }
    return count;
}

size_t StructureDiscovery::writeDPOJsonl(const std::string& path,
                                          const std::vector<std::pair<std::string, std::string>>& pairs) const {
    std::ofstream out(path);
    if (!out) return 0;

    size_t count = 0;
    for (const auto& [chosen, rejected] : pairs) {
        if (count >= config_.max_dpo_pairs) break;
        out << "{\"chosen\": " << chosen << ", \"rejected\": " << rejected << "}\n";
        count++;
    }
    return count;
}

std::vector<std::string> StructureDiscovery::generateSFTFromStructure(const DiscoveredStructure& structure) const {
    std::vector<std::string> samples;
    
    for (const auto& edge : structure.edges) {
        if (edge.confidence < config_.min_sft_quality) continue;
        
        // Generate natural language description of causal relationship
        std::string prompt = "What is the relationship between " + 
                           std::to_string(edge.from) + " and " + std::to_string(edge.to) + "?";
        std::string response = "Variable " + std::to_string(edge.from) + " " + 
                             edge.relation_type + " variable " + std::to_string(edge.to) +
                             " with confidence " + std::to_string(edge.confidence);
        
        std::ostringstream oss;
        oss << "{\"messages\": ["
            << "{\"role\": \"user\", \"content\": \"" << prompt << "\"}, "
            << "{\"role\": \"assistant\", \"content\": \"" << response << "\"}"
            << "], \"confidence\": " << edge.confidence << "}";
        
        samples.push_back(oss.str());
    }
    
    return samples;
}

std::vector<std::pair<std::string, std::string>> StructureDiscovery::generateDPOFromStructure(
    const DiscoveredStructure& structure) const {
    std::vector<std::pair<std::string, std::string>> pairs;
    
    for (const auto& edge : structure.edges) {
        if (edge.confidence < config_.min_dpo_chosen) continue;
        
        std::string prompt = "Does " + std::to_string(edge.from) + " affect " + std::to_string(edge.to) + "?";
        std::string chosen = "Yes, " + std::to_string(edge.from) + " " + edge.relation_type + " " + 
                           std::to_string(edge.to) + " (confidence: " + std::to_string(edge.confidence) + ")";
        std::string rejected = "No, there is no significant relationship between " + 
                             std::to_string(edge.from) + " and " + std::to_string(edge.to);
        
        pairs.emplace_back(chosen, rejected);
    }
    
    return pairs;
}

std::string StructureDiscovery::serialize() const {
    std::ostringstream oss;
    oss << "{\"discoveries\":" << stats_.total_discoveries
        << ",\"validations\":" << stats_.successful_validations
        << ",\"exports\":" << stats_.total_exports
        << ",\"avg_conf\":" << stats_.avg_structure_confidence
        << ",\"sft_exported\":" << stats_.total_sft_exported
        << ",\"dpo_exported\":" << stats_.total_dpo_exported << "}";
    return oss.str();
}

bool StructureDiscovery::deserialize(const std::string& data) {
    stats_ = Stats{};
    return true;
}

} // namespace sleep
} // namespace brain
} // namespace yuki