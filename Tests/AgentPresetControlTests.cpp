#include "AgentPresetControl.h"

#include <iostream>
#include <string>

namespace
{
int failures = 0;

void expectTrue (bool value, const char* name)
{
    if (value)
        return;
    ++failures;
    std::cerr << "FAIL " << name << "\n";
}

void expectEqual (const std::string& actual, const std::string& expected, const char* name)
{
    if (actual == expected)
        return;
    ++failures;
    std::cerr << "FAIL " << name << "\nexpected: " << expected
              << "\nactual:   " << actual << "\n";
}
}

int main()
{
    using namespace neuropix::agent;

    const ElectrodeMap bankA { { 0, 0, 0, 0 }, { 1, 1, 0, 0 } };
    const ElectrodeMap bankAReordered { { 1, 1, 0, 0 }, { 0, 0, 0, 0 } };
    const ElectrodeMap bankB { { 384, 0, 0, 1 }, { 385, 1, 0, 1 } };

    expectEqual (canonicalElectrodeMapHash (bankA),
                 canonicalElectrodeMapHash (bankAReordered),
                 "map hash is independent of container order");
    expectTrue (canonicalElectrodeMapHash (bankA) != canonicalElectrodeMapHash (bankB),
                "map hash changes with electrode topology");
    expectEqual (canonicalElectrodeMapHash (bankA),
                 "sha256:2bdb083f0ab65b51e840c27b7635c0892103161b17fdeb35e97e390ce75113b8",
                 "map hash is SHA-256 over canonical UTF-8 compact JSON tuples");
    expectTrue (canonicalElectrodeMapHash ({ { -1, 0, 0, 0 } }).empty(),
                "negative indices fail closed instead of hashing invalid maps");

    expectEqual (stablePresetId ("PRB2_1_4_0480_1", "Shank 1 Bank A"),
                 "prb2_1_4_0480_1:shank-1-bank-a",
                 "preset id is stable and machine safe");
    const ProbeIdentity firstProbe { { 2, 1, 1 }, "PRB2_1_4_0480_1", "1234" };
    const ProbeIdentity replacementProbe { { 2, 1, 1 }, "PRB2_1_4_0480_1", "5678" };
    expectTrue (stableCommittedStateKey (firstProbe)
                    != stableCommittedStateKey (replacementProbe),
                "replacement probe at same locator cannot inherit committed state");

    PresetInventory inventory;
    inventory.processorId = 100;
    inventory.probeId = "opaque-probe-1";
    inventory.probe = firstProbe;
    inventory.mode = ControlMode::IDLE;
    inventory.inventoryGeneration = "generation-7";
    inventory.presets = {
        { stablePresetId (inventory.probe.partNumber, "Shank 1 Bank A"), "Shank 1 Bank A", bankA },
        { stablePresetId (inventory.probe.partNumber, "Shank 1 Bank B"), "Shank 1 Bank B", bankB }
    };
    inventory.currentMap = bankA;

    auto decision = validatePresetMutation (
        inventory,
        { inventory.presets[1].presetId, "generation-7", canonicalElectrodeMapHash (bankB) });
    expectTrue (decision.allowed, "matching generation and hash allow IDLE mutation");
    expectEqual (decision.targetMapHash, canonicalElectrodeMapHash (bankB),
                 "decision identifies target map hash");

    auto staleGeneration = validatePresetMutation (
        inventory,
        { inventory.presets[1].presetId, "generation-6", canonicalElectrodeMapHash (bankB) });
    expectTrue (! staleGeneration.allowed && staleGeneration.code == "inventory_generation_mismatch",
                "stale generation fails closed");

    auto staleMap = validatePresetMutation (
        inventory,
        { inventory.presets[1].presetId, "generation-7", canonicalElectrodeMapHash (bankA) });
    expectTrue (! staleMap.allowed && staleMap.code == "electrode_map_expectation_mismatch",
                "stale map hash fails closed");

    inventory.mode = ControlMode::ACQUIRING;
    auto active = validatePresetMutation (
        inventory,
        { inventory.presets[1].presetId, "generation-7", canonicalElectrodeMapHash (bankB) });
    expectTrue (! active.allowed && active.code == "not_idle",
                "acquisition blocks mutation");

    inventory.mode = ControlMode::UNKNOWN;
    auto unknown = validatePresetMutation (
        inventory,
        { inventory.presets[1].presetId, "generation-7", canonicalElectrodeMapHash (bankB) });
    expectTrue (! unknown.allowed && unknown.code == "mode_unknown",
                "unknown mode fails closed");

    inventory.mode = ControlMode::IDLE;
    auto absent = validatePresetMutation (
        inventory,
        { "missing", "generation-7", canonicalElectrodeMapHash (bankB) });
    expectTrue (! absent.allowed && absent.code == "preset_not_supported",
                "unsupported preset fails closed");

    const auto json = serializePresetInventory (inventory);
    expectTrue (json.find ("\"inventory_generation\":\"generation-7\"") != std::string::npos,
                "inventory publishes generation");
    expectTrue (json.find ("\"probe_id\":\"opaque-probe-1\"") != std::string::npos,
                "inventory publishes probe identity");
    expectTrue (json.find ("\"electrode_map_hash\":\"sha256:") != std::string::npos,
                "inventory publishes current map hash");

    auto switched = inventory;
    switched.currentMap = bankB;
    expectEqual (presetInventoryGeneration ({ inventory }),
                 presetInventoryGeneration ({ switched }),
                 "selection changes do not invalidate inventory generation CAS");
    switched.presets.pop_back();
    expectTrue (presetInventoryGeneration ({ inventory })
                    != presetInventoryGeneration ({ switched }),
                "preset catalog changes invalidate inventory generation CAS");

    if (failures != 0)
        return 1;

    std::cout << "PASS 13 agent preset control contract checks\n";
    return 0;
}
