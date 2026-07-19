#pragma once
// ============================================================================
//  MetabolismEngine — Survival Layer (Metabolism)
//  "Electricity as food": the organism runs on four vital budgets —
//  Power (kWh), Compute (GFLOP), Storage (GB), Network (MB).
//  Exhaustion produces hunger (starving flag) which the DriveSystem converts
//  into homeostatic goals and the EconomyEngine converts into penalties.
//  Constitutional P5: every threshold is constexpr and documented.
// ============================================================================

namespace yuki::organism {

struct ResourceBudget {
    double capacityUnits    = 0.0; // maximum reserve the organism can hold
    double availableUnits   = 0.0; // current reserve
    double regenPerSecond   = 0.0; // steady replenishment (e.g. PSU inflow)
    double lifetimeConsumed = 0.0; // total ever spent (efficiency statistics)
};

struct MetabolicSnapshot {
    double powerFraction   = 1.0; // available / capacity, 0..1
    double computeFraction = 1.0;
    double storageFraction = 1.0;
    double networkFraction = 1.0;
    double viability       = 1.0; // weakest budget — the survival indicator
    bool   starving        = false; // viability < kStarvationThreshold
};

class MetabolismEngine {
public:
    // Below this fraction of the weakest budget the organism is "hungry":
    // performance degrades and homeostasis becomes the dominant drive.
    static constexpr double kStarvationThreshold = 0.15;
    // Above this fraction the organism has surplus energy (restlessness).
    static constexpr double kSatiatedThreshold   = 0.75;

    // Birth budgets — expanded later through EconomyEngine upgrades.
    static constexpr double kDefaultPowerKWh     = 1.0;   // battery/UPS reserve
    static constexpr double kDefaultComputeGflop = 500.0; // schedulable compute
    static constexpr double kDefaultStorageGb    = 10.0;  // CMF disk allowance
    static constexpr double kDefaultNetworkMb    = 512.0; // bandwidth window

    // Replenishment rates. Storage does not regenerate — it must be freed
    // explicitly (forced forgetting / releaseStorage).
    static constexpr double kPowerRegenPerSec   = 0.0003; // wall inflow
    static constexpr double kComputeRegenPerSec = 5.0;    // cores freed on idle
    static constexpr double kNetworkRegenPerSec = 0.25;   // rolling bandwidth

    MetabolismEngine();

    // Advance replenishment by dtSeconds.
    void tick(double dtSeconds);

    // Consumption. Returns false — and consumes nothing — when reserves are
    // insufficient. Callers treat a false return as a hunger signal.
    bool consumePower(double kwh);
    bool consumeCompute(double gflop);
    bool consumeStorage(double gb);
    bool consumeNetwork(double mb);

    // Forced forgetting frees storage back to the reserve.
    void releaseStorage(double gb);

    // Capacity upgrades purchased via the EconomyEngine.
    void expandPower(double kwh);
    void expandCompute(double gflop);
    void expandStorage(double gb);
    void expandNetwork(double mb);

    MetabolicSnapshot snapshot() const;

    const ResourceBudget& power()   const { return m_power; }
    const ResourceBudget& compute() const { return m_compute; }
    const ResourceBudget& storage() const { return m_storage; }
    const ResourceBudget& network() const { return m_network; }

private:
    static bool draw(ResourceBudget& budget, double units);
    static void refill(ResourceBudget& budget, double units);

    ResourceBudget m_power;
    ResourceBudget m_compute;
    ResourceBudget m_storage;
    ResourceBudget m_network;
};

} // namespace yuki::organism
