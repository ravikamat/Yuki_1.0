#include <cassert>
#include <string>
#include <map>
#include "brain/ExecutionTypes.h"
#include "brain/ScriptRunner.h"

int main() {
    ScriptRunner runner;

    std::map<std::string, std::string> args1;
    args1["script"] = "echo Hello World";

    ActionStep step1;
    step1.id = "step1";
    step1.commandOrApi = "batch";
    step1.args = args1;

    auto result1 = runner.execute(step1);
    assert(result1.success == true);
    assert(result1.exitCode == 0);

    std::map<std::string, std::string> args2;
    args2["script"] = "exit 1";

    ActionStep step2;
    step2.id = "step2";
    step2.commandOrApi = "batch";
    step2.args = args2;

    auto result2 = runner.execute(step2);
    assert(result2.success == false);

    return 0;
}
