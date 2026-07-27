#include "brain/memory/ContextManager.h"
#include <cassert>

int main() {
    yuki::memory::ContextManager cm;

    // 1. appendTurn() adds to local_messages
    cm.appendTurn("user", "Hello");
    cm.appendTurn("yuki", "Hi there");
    auto win = cm.getContextWindow();
    assert(win.local_messages.size() == 2);
    assert(win.local_messages[0].second == "Hello");

    // 2. after kLocalMax turns, oldest are compressed to summary
    for (int i = 0; i < 25; ++i) {
        cm.appendTurn("user", "Turn " + std::to_string(i));
    }
    win = cm.getContextWindow();
    assert(!win.global_summaries.empty());

    // 3. getContextWindow() returns recent N in order
    assert(!win.local_messages.empty());

    // 4. clear() empties window
    cm.clear();
    win = cm.getContextWindow();
    assert(win.local_messages.empty());
    assert(win.global_summaries.empty());

    return 0;
}
