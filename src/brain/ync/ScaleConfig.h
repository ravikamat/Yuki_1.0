#pragma once
#include "NeuromorphicSimulator.h"

namespace ync {

struct ScaleConfig {
    static SimulatorConfig mini() {
        SimulatorConfig cfg;
        cfg.num_neurons = 10000;
        cfg.num_cores = 4;
        cfg.sensory_neurons = 128;
        cfg.motor_neurons = 16;
        cfg.connectivity_density = 0.05f;
        return cfg;
    }

    static SimulatorConfig developmental() {
        SimulatorConfig cfg;
        cfg.num_neurons = 100000;
        cfg.num_cores = 4;
        cfg.sensory_neurons = 128;
        cfg.motor_neurons = 16;
        cfg.connectivity_density = 0.02f; // Sparser for scale
        return cfg;
    }

    static SimulatorConfig consolidation() {
        SimulatorConfig cfg;
        cfg.num_neurons = 1000000;
        cfg.num_cores = 4;
        cfg.sensory_neurons = 128;
        cfg.motor_neurons = 16;
        cfg.connectivity_density = 0.01f; // Very sparse
        return cfg;
    }
};

} // namespace ync
