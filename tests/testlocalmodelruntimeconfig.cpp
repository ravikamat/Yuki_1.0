#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <stdexcept>

static void test_valid_config_loading() {
    using namespace yuki::brain::platform;
    std::string testIni = "test_valid_runtime.ini";
    {
        std::ofstream out(testIni);
        out << "[oneapi]\n"
            << "enabled=true\n"
            << "oneapi_environment_script=C:/intel/setvars.bat\n"
            << "sycl_runtime_probe=sycl-ls.exe\n\n"
            << "[llama_cpp]\n"
            << "server_executable=D:/bin/llama-server.exe\n"
            << "bench_executable=D:/bin/llama-bench.exe\n"
            << "model_path=D:/models/model.gguf\n"
            << "host=127.0.0.1\n"
            << "port=18080\n"
            << "context_size=4096\n"
            << "gpu_layers=-1\n"
            << "parallel_slots=1\n"
            << "startup_timeout_ms=30000\n"
            << "request_timeout_ms=120000\n"
            << "health_timeout_ms=3000\n\n"
            << "[resource_policy]\n"
            << "foreground_cpu_reserve_logical_cores=2\n"
            << "minimum_available_ram_mb=8192\n"
            << "minimum_available_ram_mb_for_gpu_model=10240\n"
            << "maximum_background_cpu_percent=55.0\n"
            << "maximum_background_ram_percent=35.0\n"
            << "maximum_background_gpu_percent=70.0\n"
            << "idle_seconds_before_background_work=600\n\n"
            << "[model_policy]\n"
            << "require_sycl_benchmark=true\n"
            << "minimum_decode_tokens_per_second=4.0\n"
            << "maximum_health_latency_ms=3000\n"
            << "maximum_model_ram_mb=8192\n"
            << "allow_cpu_local_fallback=true\n"
            << "allow_external_fallback=true\n";
    }

    auto cfg = LocalModelRuntimeConfigLoader::load(testIni);
    assert(cfg.oneApi.enabled == true);
    assert(cfg.llamaCpp.port == 18080);
    assert(cfg.resourcePolicy.minimumAvailableRamMb == 8192);
    assert(cfg.modelPolicy.minimumDecodeTokensPerSecond == 4.0f);
    std::remove(testIni.c_str());
}

static void test_zero_port_rejection() {
    using namespace yuki::brain::platform;
    std::string testIni = "test_zero_port.ini";
    {
        std::ofstream out(testIni);
        out << "[oneapi]\nenabled=true\n[llama_cpp]\nport=0\nserver_executable=D:/bin/server.exe\nmodel_path=D:/m.gguf\n";
    }
    bool caught = false;
    try {
        LocalModelRuntimeConfigLoader::load(testIni);
    } catch (const std::exception& ex) {
        caught = true;
        (void)ex;
    }
    assert(caught);
    std::remove(testIni.c_str());
}

static void test_missing_executable_rejection() {
    using namespace yuki::brain::platform;
    std::string testIni = "test_missing_exe.ini";
    {
        std::ofstream out(testIni);
        out << "[oneapi]\nenabled=true\n[llama_cpp]\nport=18080\nserver_executable=\nmodel_path=D:/m.gguf\n";
    }
    bool caught = false;
    try {
        LocalModelRuntimeConfigLoader::load(testIni);
    } catch (const std::exception&) {
        caught = true;
    }
    assert(caught);
    std::remove(testIni.c_str());
}

static void test_invalid_percentage_rejection() {
    using namespace yuki::brain::platform;
    std::string testIni = "test_bad_pct.ini";
    {
        std::ofstream out(testIni);
        out << "[oneapi]\nenabled=true\n[llama_cpp]\nport=18080\nserver_executable=D:/server.exe\nmodel_path=D:/m.gguf\n[resource_policy]\nmaximum_background_cpu_percent=150.0\n";
    }
    bool caught = false;
    try {
        LocalModelRuntimeConfigLoader::load(testIni);
    } catch (const std::exception&) {
        caught = true;
    }
    assert(caught);
    std::remove(testIni.c_str());
}

int main() {
    std::cout << "Running testlocalmodelruntimeconfig...\n";
    test_valid_config_loading();
    test_zero_port_rejection();
    test_missing_executable_rejection();
    test_invalid_percentage_rejection();
    std::cout << "[PASS] testlocalmodelruntimeconfig completed cleanly.\n";
    return 0;
}
