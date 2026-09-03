#include "AgentNp2SopMapProductionAdapter.h"
#include "AgentNp2SopMapSimulationAdapter.h"
#include "Probes/Geometry.h"

#include <array>
#include <iostream>
#include <set>
#include <string>

namespace
{
bool sameMap (const neuropix::agent::ElectrodeMap& lhs,
              const neuropix::agent::ElectrodeMap& rhs)
{
    if (lhs.size() != rhs.size())
        return false;
    for (std::size_t i = 0; i < lhs.size(); ++i)
        if (lhs[i].channelIndex != rhs[i].channelIndex
            || lhs[i].shankIndex != rhs[i].shankIndex
            || lhs[i].bankIndex != rhs[i].bankIndex
            || lhs[i].electrodeIndex != rhs[i].electrodeIndex)
            return false;
    return true;
}

int fail (const char* message)
{
    std::cerr << message << std::endl;
    return 1;
}
}

int main()
{
    Array<ElectrodeMetadata> metadata;
    ProbeMetadata probe {};
    if (! Geometry::forPartNumber ("NP2013", metadata, probe))
        return fail ("NP2013 geometry unavailable");

    const auto production = neuropix::agent::productionNp2FourShankSopMaps (metadata);
    const auto simulation = neuropix::agent::simulationNp2FourShankSopMaps (metadata);
    if (production.size() != 8 || simulation.size() != 8)
        return fail ("SOP must expose exactly eight presets");

    const std::array<std::string, 8> labels {
        "All Shanks 1-96", "All Shanks 97-192",
        "All Shanks 193-288", "All Shanks 289-384",
        "All Shanks 385-480", "All Shanks 481-576",
        "All Shanks 577-672", "All Shanks 673-768"
    };
    std::set<std::string> hashes;
    for (std::size_t presetIndex = 0; presetIndex < production.size(); ++presetIndex)
    {
        const auto& actual = production[presetIndex];
        const auto& simulated = simulation[presetIndex];
        if (actual.label != labels[presetIndex]
            || actual.presetId != neuropix::agent::stablePresetId ("NP2013", labels[presetIndex]))
            return fail ("SOP ID/order mismatch");
        if (actual.presetId != simulated.presetId || actual.label != simulated.label
            || ! sameMap (actual.electrodeMap, simulated.electrodeMap))
            return fail ("production/simulation map mismatch");
        if (actual.electrodeMap.size() != 384)
            return fail ("each SOP map must contain 384 sites");
        std::set<int> sites;
        std::array<int, 4> perShank {};
        for (const auto& site : actual.electrodeMap)
        {
            sites.insert (site.electrodeIndex);
            if (site.shankIndex < 0 || site.shankIndex >= 4)
                return fail ("invalid shank index");
            ++perShank[static_cast<std::size_t> (site.shankIndex)];
        }
        if (sites.size() != 384 || perShank != std::array<int, 4> { 96, 96, 96, 96 })
            return fail ("SOP map must contain 96 unique sites per shank");
        hashes.insert (neuropix::agent::canonicalElectrodeMapHash (actual.electrodeMap));
    }
    if (hashes.size() != 8)
        return fail ("all eight SOP maps must have distinct hashes");

    auto incomplete = metadata;
    incomplete.remove (0);
    if (! neuropix::agent::productionNp2FourShankSopMaps (incomplete).empty()
        || ! neuropix::agent::simulationNp2FourShankSopMaps (incomplete).empty())
        return fail ("incomplete metadata must fail closed");
    if (! neuropix::agent::productionNp2FourShankSopMap (metadata, "not-a-SOP").empty()
        || ! neuropix::agent::simulationNp2FourShankSopMap (metadata, "not-a-SOP").empty())
        return fail ("unknown preset must fail closed");
    return 0;
}
