#include "YukiTestRunner.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cassert>

void YukiTestRunner::addTest(const TestCase& test) {
    tests_.push_back(test);
}

void YukiTestRunner::runAll() {
    results_.clear();
    std::cout << "\n==================================================\n";
    std::cout << " RUNNING YUKI STAGE TEST HARNESS (STAGES 0 TO 9)   \n";
    std::cout << "==================================================\n\n";

    for (const auto& test : tests_) {
        std::cout << "[RUNNING] ID " << test.id << " [" << testModeToString(test.testMode) << "]: " << test.name << " ... ";
        try {
            TestResult r = test.run();
            r.expected_pass = test.expected_pass;
            results_.push_back(r);
            
            // Check if test passed assertions
            if (r.passed == r.expected_pass) {
                std::cout << "PASSED (As Expected)\n";
            } else {
                std::cout << "UNEXPECTED STATUS (" 
                          << "Actual Passed: " << (r.passed ? "YES" : "NO") 
                          << " | Expected Passed: " << (r.expected_pass ? "YES" : "NO") 
                          << ")\n";
            }
        } catch (const std::exception& e) {
            TestResult r;
            r.id = test.id;
            r.name = test.name;
            r.stage = test.stage;
            r.category = test.category;
            r.testMode = test.testMode;
            r.expected_pass = test.expected_pass;
            r.expected = "no crash";
            r.actual = std::string("exception: ") + e.what();
            r.passed = false;
            r.notes = "Unhandled exception during test";
            results_.push_back(r);
            std::cout << "CRASHED (" << e.what() << ")\n";
        } catch (...) {
            TestResult r;
            r.id = test.id;
            r.name = test.name;
            r.stage = test.stage;
            r.category = test.category;
            r.testMode = test.testMode;
            r.expected_pass = test.expected_pass;
            r.expected = "no crash";
            r.actual = "unknown exception";
            r.passed = false;
            r.notes = "Unhandled unknown exception during test";
            results_.push_back(r);
            std::cout << "CRASHED (Unknown Exception)\n";
        }
    }
    writeReports();
}

void YukiTestRunner::writeReports() const {
    std::ofstream all("yuki_test_results_all.txt");
    std::ofstream passedFile("yuki_test_results_passed.txt");
    std::ofstream failedFile("yuki_test_results_failed.txt");
    std::ofstream summary("yuki_test_summary.txt");

    int mockPass = 0;
    int integrationPass = 0;
    int forcedFailureExpectedPass = 0;
    int unexpectedPass = 0;
    int unexpectedFail = 0;

    for (const auto& r : results_) {
        std::ostringstream line;
        line << "ID: " << r.id
             << " | Stage: " << yukiStageToString(r.stage)
             << " | Category: " << testCategoryToString(r.category)
             << " | Mode: " << testModeToString(r.testMode)
             << " | Name: " << r.name
             << " | Passed Assertions: " << (r.passed ? "YES" : "NO")
             << " | Expected Pass: " << (r.expected_pass ? "YES" : "NO")
             << "\n    Expected: " << r.expected
             << "\n    Actual:   " << r.actual
             << "\n    Notes:    " << r.notes << "\n\n";

        all << line.str();

        // Separate cleanly into yuki_test_results_passed.txt and yuki_test_results_failed.txt!
        if (r.passed) {
            passedFile << line.str();
        } else {
            failedFile << line.str();
        }

        // Tally statistics
        if (r.passed == r.expected_pass) {
            if (r.testMode == TestMode::MOCK_UNIT && r.passed) {
                mockPass++;
            } else if (r.testMode == TestMode::INTEGRATION && r.passed) {
                integrationPass++;
            } else if (r.testMode == TestMode::FORCED_FAILURE) {
                forcedFailureExpectedPass++;
            }
        } else {
            if (r.expected_pass == false && r.passed == true) {
                unexpectedPass++;
            } else if (r.expected_pass == true && r.passed == false) {
                unexpectedFail++;
            }
        }
    }

    int totalTests = (int)results_.size();
    int totalPassedAssertions = 0;
    for (const auto& r : results_) {
        if (r.passed) totalPassedAssertions++;
    }

    std::ostringstream summ;
    summ << "=============================================\n"
         << "           YUKI TEST HARNESS SUMMARY         \n"
         << "=============================================\n"
         << "Total Tests Run:           " << totalTests << "\n"
         << "Mock Pass Count:           " << mockPass << "\n"
         << "Integration Pass Count:    " << integrationPass << "\n"
         << "Forced-Failure Expected:   " << forcedFailureExpectedPass << "\n"
         << "Unexpected Pass (Anomaly): " << unexpectedPass << "\n"
         << "Unexpected Fail (Anomaly): " << unexpectedFail << "\n"
         << "---------------------------------------------\n"
         << "Passed Assertions Count:   " << totalPassedAssertions << " / " << totalTests << "\n"
         << "Failed Assertions Count:   " << (totalTests - totalPassedAssertions) << " / " << totalTests << "\n"
         << "Assertion Pass Rate:       " << (totalTests > 0 ? (100.0 * totalPassedAssertions / totalTests) : 0.0) << "%\n"
         << "=============================================\n";

    summary << summ.str();
    std::cout << "\n" << summ.str() << "\n";
}
