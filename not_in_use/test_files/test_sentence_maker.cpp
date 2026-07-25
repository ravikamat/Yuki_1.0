#include "brain/language/SentenceMaker.h"
#include <cassert>
#include <fstream>
#include <unordered_map>

int main() {
    yuki::language::SentenceMaker sm;

    // 1. loadTemplates() with missing file -> false (no crash)
    assert(!sm.loadTemplates("non_existent_grammar_file.txt"));

    // 2. loadTemplates() with valid file -> true
    {
        std::ofstream out("test_grammar_temp.txt");
        out << "T1\tHello {name}, welcome to {place}!\n";
        out << "T2\tThe result is {value}.\n";
    }
    assert(sm.loadTemplates("test_grammar_temp.txt"));

    // 3. compose() replaces slots correctly
    std::unordered_map<std::string, std::string> slots = {
        {"name", "Alice"},
        {"place", "Yuki"}
    };
    std::string res1 = sm.compose("T1", slots);
    assert(res1 == "Hello Alice, welcome to Yuki!");

    // 4. compose() with missing template returns empty string
    assert(sm.compose("MISSING_TEMPLATE", slots).empty());

    // 5. compose() with missing slots leaves placeholder intact
    std::unordered_map<std::string, std::string> partial_slots = {{"name", "Bob"}};
    std::string res2 = sm.compose("T1", partial_slots);
    assert(res2 == "Hello Bob, welcome to {place}!");

    // 6. template count matches file line count
    assert(sm.templateCount() == 2);

    std::remove("test_grammar_temp.txt");
    return 0;
}
