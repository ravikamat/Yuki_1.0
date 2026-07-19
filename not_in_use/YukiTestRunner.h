#pragma once
#include "YukiTestTypes.h"
#include <functional>
#include <vector>
#include <string>

struct TestCase {
    int id;
    std::string name;
    YukiStage stage;
    TestCategory category;
    TestMode testMode;
    bool expected_pass = true; // Whether the test is expected to pass (true) or fail (false)
    std::function<TestResult()> run;
};

class YukiTestRunner {
public:
    void addTest(const TestCase& test);
    void runAll();

private:
    void writeReports() const;
    std::vector<TestCase> tests_;
    std::vector<TestResult> results_;
};
