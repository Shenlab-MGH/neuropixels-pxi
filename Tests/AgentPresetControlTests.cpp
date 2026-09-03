#include "AgentPresetControl.h"

#include <iostream>
#include <set>
#include <string>
#include <thread>

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
    expectTrue (isPresetTargetPublishable (ProbeStatus::CONNECTED, true, true, false),
                "only connected valid enabled probes are publishable");
    expectTrue (isPresetTargetPublishable (ProbeStatus::UPDATING, true, true, false),
                "UPDATING remains readable when a committed acknowledgment exists");
    expectTrue (! isPresetTargetPublishable (ProbeStatus::DISCONNECTED, true, true, false)
                    && ! isPresetTargetPublishable (ProbeStatus::CONNECTED, false, true, false)
                    && ! isPresetTargetPublishable (ProbeStatus::CONNECTED, true, false, false)
                    && ! isPresetTargetPublishable (ProbeStatus::CONNECTED, true, true, true),
                "disconnected invalid unsupported and disabled probes are omitted");

    expectTrue (controlModeFromCoreState (true, true, ProbeStatus::CONNECTED)
                    == ControlMode::RECORDING,
                "authoritative recording state takes precedence over acquisition");
    expectTrue (controlModeFromCoreState (true, false, ProbeStatus::CONNECTED)
                    == ControlMode::ACQUIRING,
                "authoritative acquisition state is reported separately from connection status");
    expectTrue (controlModeFromCoreState (false, false, ProbeStatus::CONNECTED)
                    == ControlMode::IDLE,
                "connected probe with inactive core is idle");
    expectTrue (controlModeFromCoreState (false, false, ProbeStatus::UPDATING)
                    == ControlMode::UNKNOWN,
                "non-connected probe state fails closed when the core is idle");

    CommittedPresetStateCache cache;
    const auto cacheKey = stableCommittedStateKey (firstProbe);
    const auto connectionEpoch = cache.connectionEpoch();
    expectTrue (! cache.resolve (cacheKey).has_value(),
                "connected software settings are not published without SDK acknowledgment");
    expectTrue (! cache.resolveForPublication (cacheKey, ProbeStatus::UPDATING,
                                                true, true, false).has_value(),
                "UPDATING without a committed acknowledgment remains omitted");
    cache.commit (cacheKey, bankA);
    expectTrue (! shouldInvalidateCommittedState (ProbeStatus::UPDATING),
                "UPDATING retains last committed acknowledgment");
    expectEqual (canonicalElectrodeMapHash (*cache.resolve (cacheKey)),
                 canonicalElectrodeMapHash (bankA),
                 "failed write desired state cannot replace committed A after UPDATING");
    expectEqual (canonicalElectrodeMapHash (*cache.resolve (cacheKey)),
                 canonicalElectrodeMapHash (bankA),
                 "sustained UPDATING continues to publish only committed A");
    expectEqual (canonicalElectrodeMapHash (*cache.resolveForPublication (
                     cacheKey, ProbeStatus::UPDATING, true, true, false)),
                 canonicalElectrodeMapHash (bankA),
                 "UPDATING with an acknowledgment publishes committed A");
    expectTrue (shouldInvalidateCommittedState (ProbeStatus::DISCONNECTED),
                "true disconnect invalidates committed acknowledgment");
    expectTrue (shouldInvalidateCommittedState (ProbeStatus::CONNECTING),
                "new connection epoch invalidates prior acknowledgment");
    cache.observeIdentityLost (cacheKey);
    expectTrue (! cache.resolve (cacheKey).has_value(),
                "disconnect then failed restore remains unpublished without a new SDK acknowledgment");

    cache.commit (cacheKey, bankA);
    cache.observeIdentity (replacementProbe);
    expectTrue (! cache.resolve (cacheKey).has_value(),
                 "replacement serial removes prior identity at the same locator");

    cache.commit (cacheKey, bankA);
    cache.retainOnly ({ stableCommittedStateKey (replacementProbe) });
    expectTrue (! cache.resolve (cacheKey).has_value(),
                 "identity removal from inventory removes stale committed state");

    cache.commit (cacheKey, bankA);
    cache.invalidateConnectionEpoch();
    expectTrue (! cache.resolve (cacheKey).has_value(),
                 "hardware refresh connection epoch cannot retain an old acknowledgment");
    expectTrue (! cache.commitIfEpoch (cacheKey, bankA, connectionEpoch),
                "delayed old-epoch SDK success cannot commit after refresh");
    expectTrue (! cache.resolve (cacheKey).has_value(),
                "delayed old-epoch commit leaves refreshed target unpublished");
    cache.commit (cacheKey, bankB);
    expectEqual (canonicalElectrodeMapHash (*cache.resolve (cacheKey)),
                 canonicalElectrodeMapHash (bankB),
                 "successful SDK commit is required before target publication");

    PresetHardwareOperationGate operationGate;
    expectTrue (operationGate.tryBeginRefresh(),
                "idle hardware operation gate starts refresh");
    int queuedDispatches = 0;
    if (operationGate.tryBeginApply())
        ++queuedDispatches;
    expectTrue (queuedDispatches == 0,
                "refresh-active SET performs zero settings dispatches");
    operationGate.finishRefresh();
    expectTrue (operationGate.tryBeginApply(),
                "idle hardware operation gate starts preset apply");
    expectTrue (! operationGate.tryBeginRefresh(),
                "preset apply blocks close-open hardware refresh");
    operationGate.finishApply();

    expectTrue (operationGate.tryBeginSettingsWorker(),
                "ordinary settings worker enters the shared operation domain");
    expectTrue (! operationGate.tryAcquireInventory().has_value()
                    && ! operationGate.tryBeginApply()
                    && ! operationGate.tryBeginRefresh(),
                "ordinary settings worker excludes inventory agent SET and refresh");
    expectTrue (operationGate.tryContinueSettingsWorker(),
                "one GUI settings batch may enqueue multiple probes");
    operationGate.finishSettingsWorker();
    {
        auto inventoryLease = operationGate.tryAcquireInventory();
        expectTrue (inventoryLease.has_value()
                        && ! operationGate.tryBeginSettingsWorker(),
                    "inventory lease prevents a real settings worker start");
    }

    {
        auto inventoryLease = operationGate.tryAcquireInventory();
        expectTrue (inventoryLease.has_value(),
                    "inventory acquires the shared hardware-state synchronization domain");
        bool concurrentApplyStarted = true;
        std::thread concurrentApply ([&]
        {
            concurrentApplyStarted = operationGate.tryBeginApply();
        });
        concurrentApply.join();
        expectTrue (! concurrentApplyStarted,
                    "inventory snapshot excludes a concurrent preset apply");
    }
    expectTrue (operationGate.tryBeginApply(),
                "inventory lease releases the synchronization domain");
    expectTrue (! operationGate.tryAcquireInventory().has_value(),
                "active preset apply makes inventory stably busy");
    operationGate.finishApply();

    const std::vector<std::string> expectedSopPresets {
        "All Shanks 1-96", "All Shanks 97-192",
        "All Shanks 193-288", "All Shanks 289-384",
        "All Shanks 385-480", "All Shanks 481-576",
        "All Shanks 577-672", "All Shanks 673-768"
    };
    const std::vector<std::string> expectedSopPresetIds {
        "np2013:all-shanks-1-96", "np2013:all-shanks-97-192",
        "np2013:all-shanks-193-288", "np2013:all-shanks-289-384",
        "np2013:all-shanks-385-480", "np2013:all-shanks-481-576",
        "np2013:all-shanks-577-672", "np2013:all-shanks-673-768"
    };
    expectTrue (np2FourShankSopPresetLabels() == expectedSopPresets,
                "NP2 four-shank inventory pins the existing eight-block SOP order");
    std::set<std::string> actualSopMapHashes;
    for (std::size_t block = 0; block < expectedSopPresets.size(); ++block)
    {
        const auto indices = np2FourShankSopElectrodeIndices (expectedSopPresets[block]);
        expectTrue (indices.size() == 384
                        && indices.front() == static_cast<int> (block * 96)
                        && indices.back() == static_cast<int> (block * 96 + 3 * 1280 + 95),
                    "production and simulation use the exact SOP electrode map");
        ElectrodeMap actualMap;
        for (std::size_t channel = 0; channel < indices.size(); ++channel)
            actualMap.push_back ({ static_cast<int> (channel),
                                   static_cast<int> (channel / 96),
                                   static_cast<int> (block), indices[channel] });
        actualSopMapHashes.insert (canonicalElectrodeMapHash (actualMap));
    }
    expectTrue (actualSopMapHashes.size() == 8,
                "the eight actual SOP electrode maps have distinct hashes");
    expectTrue (np2FourShankSopElectrodeIndices ("All Shanks 769-864").empty(),
                "non-SOP electrode maps fail closed");
    std::vector<Preset> shuffledSopCandidates;
    for (auto label = expectedSopPresets.rbegin(); label != expectedSopPresets.rend(); ++label)
        shuffledSopCandidates.push_back ({ stablePresetId ("NP2013", *label),
                                           *label, bankA });
    shuffledSopCandidates.push_back ({ "ignored:single", "Shank 1 Bank A", bankB });
    shuffledSopCandidates.push_back ({ "ignored:ninth", "All Shanks 769-864", bankB });
    const auto filteredSopPresets = retainNp2FourShankSopPresets (shuffledSopCandidates);
    expectTrue (filteredSopPresets.size() == 8,
                "NP2 four-shank publishes exactly eight SOP presets");
    for (std::size_t index = 0; index < filteredSopPresets.size(); ++index)
    {
        expectEqual (filteredSopPresets[index].label, expectedSopPresets[index],
                     "SOP preset order is stable");
        expectEqual (filteredSopPresets[index].presetId, expectedSopPresetIds[index],
                     "SOP preset id is stable");
        expectEqual (canonicalElectrodeMapHash (filteredSopPresets[index].electrodeMap),
                     "sha256:2bdb083f0ab65b51e840c27b7635c0892103161b17fdeb35e97e390ce75113b8",
                     "SOP preset map hash is preserved exactly");
    }
    shuffledSopCandidates.pop_back();
    shuffledSopCandidates.erase (shuffledSopCandidates.begin());
    expectTrue (retainNp2FourShankSopPresets (shuffledSopCandidates).empty(),
                "incomplete eight-preset catalog fails closed");

    {
        auto refreshLease = operationGate.tryAcquireRefresh();
        expectTrue (refreshLease.has_value()
                        && operationGate.refreshInProgress(),
                    "refresh lease owns gate for its full scope");
        auto movedLease = std::move (*refreshLease);
        refreshLease.reset();
        expectTrue (operationGate.refreshInProgress(),
                    "moving refresh lease does not release gate early");
    }
    expectTrue (operationGate.tryBeginApply(),
                "refresh lease scope releases gate after failed or completed launch");
    operationGate.finishApply();

    const std::vector<PresetSourceBinding> filteredBindings {
        { "publishable-probe-1", "SERIAL-B", 1 }
    };
    expectTrue (resolvePresetSource (filteredBindings,
                                     "publishable-probe-1",
                                     "SERIAL-B").sourceIndex == 1,
                "omitted probe zero cannot redirect probe one mutation");
    const std::vector<PresetSourceBinding> multipleOmittedBindings {
        { "publishable-probe-3", "SERIAL-D", 3 }
    };
    expectTrue (resolvePresetSource (multipleOmittedBindings,
                                     "publishable-probe-3",
                                     "SERIAL-D").sourceIndex == 3,
                "multiple omitted probes preserve original source index");
    expectTrue (resolvePresetSource ({ { "duplicate", "A", 0 },
                                       { "duplicate", "B", 2 } },
                                     "duplicate", "A").code == "ambiguous_target",
                "duplicate fresh probe id fails closed");
    expectTrue (resolvePresetSource (filteredBindings,
                                     "publishable-probe-1",
                                     "WRONG").code == "probe_identity_mismatch",
                "fresh serial mismatch fails before source dispatch");

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
    expectEqual (serializeAgentSuccess ("{\"inventory_generation\":\"g1\",\"targets\":[]}"),
                 "{\"ok\":true,\"result\":{\"inventory_generation\":\"g1\",\"targets\":[]}}",
                 "success wrapper has exact core envelope and no extra keys");
    expectEqual (serializeAgentAccepted ("neuropix-preset-9"),
                 "{\"ok\":true,\"accepted\":true,\"operation_id\":\"neuropix-preset-9\"}",
                 "set no-op and write dispatch share exact accepted envelope");

    auto switched = inventory;
    switched.currentMap = bankB;
    expectEqual (presetInventoryGeneration ({ inventory }),
                 presetInventoryGeneration ({ switched }),
                 "selection changes do not invalidate inventory generation CAS");
    cache.commit (cacheKey, bankA);
    auto updatingReadback = inventory;
    updatingReadback.currentMap = *cache.resolveForPublication (
        cacheKey, ProbeStatus::UPDATING, true, true, false);
    cache.commit (cacheKey, bankB);
    auto connectedReadback = inventory;
    connectedReadback.currentMap = *cache.resolveForPublication (
        cacheKey, ProbeStatus::CONNECTED, true, true, false);
    expectEqual (presetInventoryGeneration ({ updatingReadback }),
                 presetInventoryGeneration ({ connectedReadback }),
                 "accepted apply polls old then new acknowledgment under one generation");
    expectTrue (canonicalElectrodeMapHash (updatingReadback.currentMap)
                    != canonicalElectrodeMapHash (connectedReadback.currentMap),
                "UPDATING old acknowledgment advances only after successful SDK commit");
    switched.presets.pop_back();
    expectTrue (presetInventoryGeneration ({ inventory })
                    != presetInventoryGeneration ({ switched }),
                "preset catalog changes invalidate inventory generation CAS");

    if (failures != 0)
        return 1;

    std::cout << "PASS 13 agent preset control contract checks\n";
    return 0;
}
