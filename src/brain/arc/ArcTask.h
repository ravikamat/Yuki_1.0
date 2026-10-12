#pragma once

#include "ArcTypes.h"
#include <vector>
#include <string>
#include <optional>
#include <fstream>
#include <nlohmann/json.hpp>

namespace yuki {
namespace brain {
namespace arc {

struct Example {
    Grid input;
    Grid output;
    
    Example() = default;
    Example(const Grid& in, const Grid& out) : input(in), output(out) {}
    
    bool operator==(const Example& other) const {
        return gridsEqual(input, other.input) && gridsEqual(output, other.output);
    }
};

struct Task {
    std::string id;
    std::vector<Example> train;
    std::vector<Example> test;
    
    Task() = default;
    Task(const std::string& task_id) : id(task_id) {}
    
    size_t numTrain() const { return train.size(); }
    size_t numTest() const { return test.size(); }
    
    bool hasTestOutput(size_t idx) const {
        return idx < test.size() && !test[idx].output.empty();
    }
};

inline Task parseTask(const nlohmann::json& json, const std::string& task_id = "") {
    Task task;
    task.id = task_id.empty() ? json.value("id", "unknown") : task_id;
    
    if (json.contains("train")) {
        for (const auto& ex : json["train"]) {
            Grid input = stringToGrid(ex["input"].dump());
            Grid output = stringToGrid(ex["output"].dump());
            task.train.emplace_back(input, output);
        }
    }
    
    if (json.contains("test")) {
        for (const auto& ex : json["test"]) {
            Grid input = stringToGrid(ex["input"].dump());
            Grid output;
            if (ex.contains("output")) {
                output = stringToGrid(ex["output"].dump());
            }
            task.test.emplace_back(input, output);
        }
    }
    
    return task;
}

inline std::vector<Task> parseTasks(const nlohmann::json& json) {
    std::vector<Task> tasks;
    if (json.is_object()) {
        for (const auto& [id, task_json] : json.items()) {
            tasks.push_back(parseTask(task_json, id));
        }
    } else if (json.is_array()) {
        for (size_t i = 0; i < json.size(); ++i) {
            tasks.push_back(parseTask(json[i], std::to_string(i)));
        }
    }
    return tasks;
}

inline std::optional<std::vector<Task>> loadTasksFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return std::nullopt;
    
    try {
        nlohmann::json json;
        file >> json;
        return parseTasks(json);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace arc
} // namespace brain
} // namespace yuki