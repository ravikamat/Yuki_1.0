#include "LearningUpdate.h"
#include "brain/retrieval/RetrievalSystem.h"
#include "SmartScraper.h"
#include <iostream>
#include <algorithm>

LearningUpdate::LearningUpdate(UncertaintyDetector& ud) : uncertaintyDetectorRef(ud) {}

void LearningUpdate::finalizeAndLearn(const MeaningState& state, bool success) {
    if (!success) {
        // If the execution or clarification failed, we don't learn from this hypothesis
        return;
    }
    
    // If successful, and we used a candidate substitution that worked, 
    // we should reinforce it.
    // For the blank-start model, if the user explicitly confirms a new word,
    // we add it to the vocabulary.
    
    // Example: If the user says "Yes, I meant Einstein"
    // We would extract "Einstein" and add it to our dictionary.
    // For now, if there were any linked entities that were successfully used,
    // ensure their canonical forms are in our vocab.
    for (const auto& entity : state.entities) {
        if (entity.type != EntityType::UNKNOWN_ENTITY) {
            uncertaintyDetectorRef.learnWord(entity.canonical_form);
        }
    }
}

std::string LearningUpdate::extractDefinition(const std::string& word, const std::string& text) {
    if (text.empty()) return "";
    
    // Very simple C++ NLP: look for "X is a", "X means", "X refers to"
    std::string lowerText = text;
    std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
    std::string lowerWord = word;
    std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), ::tolower);
    
    std::vector<std::string> patterns = {
        lowerWord + " is a ",
        lowerWord + " is an ",
        lowerWord + " means ",
        lowerWord + " refers to ",
        lowerWord + " stands for "
    };
    
    for (const auto& pattern : patterns) {
        size_t pos = lowerText.find(pattern);
        if (pos != std::string::npos) {
            // Extract the sentence
            size_t endPos = lowerText.find('.', pos);
            if (endPos == std::string::npos) endPos = lowerText.find('\n', pos);
            if (endPos == std::string::npos) endPos = pos + 200; // Cap
            
            if (endPos > pos && endPos < lowerText.size()) {
                // Return original case
                return text.substr(pos, endPos - pos + 1);
            }
        }
    }
    
    // Fallback: Just return the first 150 chars if the word is in the text
    size_t pos = lowerText.find(lowerWord);
    if (pos != std::string::npos) {
        size_t start = (pos > 50) ? pos - 50 : 0;
        size_t end = std::min(text.size(), pos + 150);
        return "Context: ..." + text.substr(start, end - start) + "...";
    }
    
    return "";
}

bool LearningUpdate::autoResearch(const std::string& unknownWord) {
    std::cout << "[LearningUpdate] Triggered Autonomous Research for: '" << unknownWord << "'\n";
    
    WebReconAgent searcher;
    if (!searcher.init()) {
        std::cout << "[LearningUpdate] WebReconAgent init failed.\n";
        return false;
    }
    
    std::cout << "[LearningUpdate] Searching DuckDuckGo for: " << unknownWord << " definition\n";
    auto urls = searcher.searchDuckDuckGoUrls(unknownWord + " definition", 3);
    
    if (urls.empty()) {
        std::cout << "[LearningUpdate] No URLs found for " << unknownWord << "\n";
        return false;
    }
    
    SmartScraper scraper;
    
    for (const auto& url : urls) {
        std::cout << "[LearningUpdate] Scraping: " << url << "\n";
        std::string html = scraper.fetchHtml(url);
        std::string text = scraper.extractSemanticText(html);
        
        std::string def = extractDefinition(unknownWord, text);
        if (!def.empty()) {
            std::cout << "[LearningUpdate] SUCCESS! Learned definition:\n>>> " << def << "\n";
            uncertaintyDetectorRef.learnWord(unknownWord);
            return true;
        }
    }
    
    std::cout << "[LearningUpdate] Failed to extract a clear definition.\n";
    return false;
}
