#include "input/InputAnalyzer.h"
#include <cassert>
#include <string>

int main() {
    yuki::input::InputAnalyzer ia;

    // 1. normalizeUnicode() strips BOM
    std::string text_bom = "\xEF\xBB\xBFHello World";
    ia.normalizeUnicode(text_bom);
    assert(text_bom == "Hello World");

    // 2. stripWhitespace() trims and collapses
    std::string text_ws = "  Hello   World  \n\t ";
    ia.stripWhitespace(text_ws);
    assert(text_ws == "Hello World");

    // 3. detectCommandPrefix() recognizes loaded prefixes
    assert(ia.detectCommandPrefix("open app") == "open");
    assert(ia.detectCommandPrefix("run script") == "run");

    // 4. detectCommandPrefix() returns empty for unknown prefix
    assert(ia.detectCommandPrefix("sing a song").empty());

    // 5. classifyInputType("What?") -> QUESTION
    assert(ia.classifyInputType("What is your name?") == yuki::input::InputType::QUESTION);

    // 6. classifyInputType("Open chrome") -> COMMAND
    assert(ia.classifyInputType("Open chrome") == yuki::input::InputType::COMMAND);

    return 0;
}
