#include "MassCurriculumLoader.h"
#include "../memory/CognitiveMemoryFabric.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

namespace yuki {
namespace learning {

bool MassCurriculumLoader::isCompleted() {
    return fs::exists("data/curriculum/.mass_complete");
}

MassCurriculumLoader::MassCurriculumLoader(std::shared_ptr<memory::CognitiveMemoryFabric> cmf)
    : cmf_(std::move(cmf)) {}

MassCurriculumLoader::~MassCurriculumLoader() = default;

void MassCurriculumLoader::execute() {
    if (isCompleted()) {
        std::cout << "[MassCurriculum] Already completed. Skipping.\n";
        return;
    }

    running_ = true;
    fs::create_directories("data/curriculum/bootstrap");
    fs::create_directories("data/curriculum/mass");

    generateBootstrapDefaults();

    auto topics = getTopics();
    size_t total = topics.size();
    size_t ingested = 0;

    for (size_t i = 0; i < total; ++i) {
        std::cout << "[MassCurriculum] Topic " << (i+1) << "/" << total 
                  << ": " << topics[i].name << "\n";

        // Try local files first (bootstrap)
        for (const auto& file : topics[i].local_files) {
            if (fs::exists(file)) {
                ingestLocalFile(file, topics[i].tag);
                ingested++;
            }
        }

        progress_ = static_cast<double>(i + 1) / static_cast<double>(total);
    }

    writeCompletionFlag();
    running_ = false;
    std::cout << "[MassCurriculum] Complete. Ingested " << ingested << " samples.\n";
}

void MassCurriculumLoader::generateBootstrapDefaults() {
    struct BootItem { std::string file; std::string content; };
    std::vector<BootItem> defaults = {
        {"data/curriculum/bootstrap/english_lit.txt",
         "English literature includes works by Shakespeare, Austen, and Dickens. "
         "Reading comprehension requires understanding plot, character, and theme. "
         "A novel is a long fictional narrative. Poetry uses meter and rhyme."},
        {"data/curriculum/bootstrap/grammar.txt",
         "English grammar consists of syntax, morphology, and punctuation. "
         "A sentence has a subject, predicate, and object. "
         "Nouns name things. Verbs express actions. Adjectives describe nouns."},
        {"data/curriculum/bootstrap/vocabulary.txt",
         "Empathy is understanding others feelings. Resilience is recovering from adversity. "
         "Algorithm is a step-by-step procedure. Syntax is rules of language structure. "
         "Abstraction hides complexity. Encapsulation bundles data and methods."},
        {"data/curriculum/bootstrap/psychology.txt",
         "Psychology studies mind and behavior. Key concepts include cognition, emotion, "
         "perception, memory, learning, and social interaction. "
         "Cognitive dissonance is mental conflict from contradictory beliefs."},
        {"data/curriculum/bootstrap/dialogues.txt",
         "Active listening means paying full attention, reflecting, and clarifying. "
         "Empathetic response validates the speakers emotional state. "
         "Open-ended questions encourage detailed answers."},
        {"data/curriculum/bootstrap/math.txt",
         "Probability measures likelihood. Statistics analyzes data. "
         "Linear algebra deals with vectors and matrices. Calculus studies change and rates. "
         "A derivative is the rate of change of a function."},
        {"data/curriculum/bootstrap/algorithms.txt",
         "Quicksort divides and conquers with average order n log n complexity. "
         "Dynamic programming solves subproblems once and stores results. "
         "A graph consists of nodes and edges. Dijkstra finds shortest paths."},
        {"data/curriculum/bootstrap/code.txt",
         "C++ uses headers and implementation files. Classes encapsulate data and behavior. "
         "Pointers store memory addresses. RAII manages resource lifetime automatically. "
         "Inheritance allows classes to derive from base classes."},
        {"data/curriculum/bootstrap/general.txt",
         "The scientific method involves hypothesis, experiment, analysis, and conclusion. "
         "Critical thinking evaluates evidence before accepting claims. "
         "Occams razor prefers simpler explanations."},
    };

    for (const auto& item : defaults) {
        if (!fs::exists(item.file)) {
            std::ofstream ofs(item.file);
            if (ofs) ofs << item.content;
        }
    }
}

void MassCurriculumLoader::ingestLocalFile(const std::string& path, const std::string& topic) {
    std::ifstream ifs(path);
    if (!ifs) return;

    std::string text((std::istreambuf_iterator<char>(ifs)),
                      std::istreambuf_iterator<char>());

    if (cmf_) {
        yuki::memory::MemoryPacket pkt;
        pkt.type = yuki::memory::MemoryPacket::KNOWLEDGE_FACT;
        pkt.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        pkt.source = "mass_curriculum";
        pkt.text = text;
        pkt.confidence = 0.8f;
        pkt.topic_tag = topic;
        cmf_->ingest(pkt);
    }
}

void MassCurriculumLoader::writeCompletionFlag() {
    std::ofstream flag("data/curriculum/.mass_complete");
    if (flag) flag << "completed\n";
}

std::vector<CurriculumTopic> MassCurriculumLoader::getTopics() const {
    return {
        {"english_literature", "english",
         {}, {"data/curriculum/bootstrap/english_lit.txt"}, 500},
        {"english_grammar", "grammar",
         {}, {"data/curriculum/bootstrap/grammar.txt"}, 500},
        {"vocabulary", "vocab",
         {}, {"data/curriculum/bootstrap/vocabulary.txt"}, 1000},
        {"human_psychology", "psychology",
         {}, {"data/curriculum/bootstrap/psychology.txt"}, 500},
        {"dialogue_patterns", "dialogue",
         {}, {"data/curriculum/bootstrap/dialogues.txt"}, 500},
        {"mathematics", "math",
         {}, {"data/curriculum/bootstrap/math.txt"}, 500},
        {"algorithms", "algorithms",
         {}, {"data/curriculum/bootstrap/algorithms.txt"}, 500},
        {"code_understanding", "code",
         {}, {"data/curriculum/bootstrap/code.txt"}, 500},
        {"general_knowledge", "general",
         {}, {"data/curriculum/bootstrap/general.txt"}, 500},
    };
}

} // namespace learning
} // namespace yuki
