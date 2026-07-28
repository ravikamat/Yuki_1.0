#include "brain/language/GrammarExtractor.h"
#include <iostream>
#include <fstream>
#include <cassert>

int main() {
    std::cout << "[TEST] GrammarExtractor..." << std::endl;

    yuki::language::GrammarExtractor extractor;
    bool ok = extractor.parseBracketedLine("(S (NP (DT the) (NN dog)) (VP (VB runs)))");
    assert(ok);
    assert(extractor.ruleCount() > 0);
    assert(extractor.lexicalCount() > 0);

    std::string test_corpus = "test_corpus.txt";
    std::ofstream out(test_corpus);
    out << "(S (NP (DT a) (NN cat)) (VP (VB sleeps)))\n";
    out.close();

    bool ok_file = extractor.parseFile(test_corpus);
    assert(ok_file);

    extractor.exportToGrammarEngine("test_frames.txt", "test_rules.txt", "test_lexicon.txt");

    std::remove(test_corpus.c_str());
    std::remove("test_frames.txt");
    std::remove("test_rules.txt");
    std::remove("test_lexicon.txt");

    std::cout << "[TEST] GrammarExtractor PASSED!" << std::endl;
    return 0;
}
