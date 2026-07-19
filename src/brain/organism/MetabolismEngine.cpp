#include "MetabolismEngine.h"

#include <algorithm>

namespace yuki::organism {

MetabolismEngine::MetabolismEngine() {
    m_power.capacityUnits  = kDefaultPowerKWh;
    m_power.availableUnits = kDefaultPowerKWh;
    m_power.regenPerSecond = kPowerRegenPerSec;

    m_compute.capacityUnits  = kDefaultComputeGflop;
    m_compute.availableUnits = kDefaultComputeGflop;
    m_compute.regenPerSecond = kComputeRegenPerSec;

    m_storage.capacityUnits  = kDefaultStorageGb;
    m_storage.availableUnits = kDefaultStorageGb;
    m_storage.regenPerSecond = 0.0; // storage is only freed explicitly

    m_network.capacityUnits  = kDefaultNetworkMb;
    m_network.availableUnits = kDefaultNetworkMb;
    m_network.regenPerSecond = kNetworkRegenPerSec;
}

void MetabolismEngine::tick(double dtSeconds) {
    if (dtSeconds <= 0.0) return;
    refill(m_power,   m_power.regenPerSecond   * dtSeconds);
    refill(m_compute, m_compute.regenPerSecond * dtSeconds);
    refill(m_network, m_network.regenPerSecond * dtSeconds);
}

bool MetabolismEngine::draw(ResourceBudget& budget, double units) {
    if (units <= 0.0) return true;
    if (budget.availableUnits < units) return false;
    budget.availableUnits   -= units;
    budget.lifetimeConsumed += units;
    return true;
}

void MetabolismEngine::refill(ResourceBudget& budget, double units) {
    if (units <= 0.0) return;
    budget.availableUnits = std::min(budget.capacityUnits, budget.availableUnits + units);
}

bool MetabolismEngine::consumePower(double kwh)     { return draw(m_power, kwh); }
bool MetabolismEngine::consumeCompute(double gflop) { return draw(m_compute, gflop); }
bool MetabolismEngine::consumeStorage(double gb)    { return draw(m_storage, gb); }
bool MetabolismEngine::consumeNetwork(double mb)    { return draw(m_network, mb); }

void MetabolismEngine::releaseStorage(double gb) { refill(m_storage, gb); }

void MetabolismEngine::expandPower(double kwh)     { m_power.capacityUnits += kwh;     m_power.availableUnits += kwh; }
void MetabolismEngine::expandCompute(double gflop) { m_compute.capacityUnits += gflop; m_compute.availableUnits += gflop; }
void MetabolismEngine::expandStorage(double gb)    { m_storage.capacityUnits += gb;    m_storage.availableUnits += gb; }
void MetabolismEngine::expandNetwork(double mb)    { m_network.capacityUnits += mb;    m_network.availableUnits += mb; }

MetabolicSnapshot MetabolismEngine::snapshot() const {
    const auto fraction = [](const ResourceBudget& b) {
        return (b.capacityUnits > 0.0) ? (b.availableUnits / b.capacityUnits) : 0.0;
    };
    MetabolicSnapshot s;
    s.powerFraction   = fraction(m_power);
    s.computeFraction = fraction(m_compute);
    s.storageFraction = fraction(m_storage);
    s.networkFraction = fraction(m_network);
    s.viability = std::min(std::min(s.powerFraction, s.computeFraction),
                           std::min(s.storageFraction, s.networkFraction));
    s.starving  = s.viability < kStarvationThreshold;
    return s;
}

} // namespace yuki::organism
