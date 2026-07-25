// YNCCheckpoint.cpp — static save/load wrapper + directory scan.
#include "YNCCheckpoint.h"
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

namespace ync {

bool YNCCheckpoint::save(const NeuromorphicSimulator& sim, const std::string& path) {
    sim.saveCheckpoint(path);
    std::ifstream verify(path, std::ios::binary);
    return verify.good();
}

bool YNCCheckpoint::load(NeuromorphicSimulator& sim, const std::string& path) {
    return sim.loadCheckpoint(path);
}

std::vector<std::string> YNCCheckpoint::listCheckpoints(const std::string& directory) {
    std::vector<std::string> results;
#ifdef _WIN32
    std::string search_path = directory + "\\*.ynck";
    WIN32_FIND_DATAA find_data;
    HANDLE handle = FindFirstFileA(search_path.c_str(), &find_data);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            results.push_back(directory + "\\" + find_data.cFileName);
        } while (FindNextFileA(handle, &find_data));
        FindClose(handle);
    }
#else
    DIR* dir = opendir(directory.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name.size() > 5 && name.substr(name.size() - 5) == ".ynck") {
                results.push_back(directory + "/" + name);
            }
        }
        closedir(dir);
    }
#endif
    return results;
}

} // namespace ync
