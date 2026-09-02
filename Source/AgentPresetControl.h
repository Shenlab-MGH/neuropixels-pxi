#pragma once

#include "AgentInventory.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace neuropix::agent
{
struct ElectrodeSite
{
    int channelIndex;
    int shankIndex;
    int bankIndex;
    int electrodeIndex;
};

using ElectrodeMap = std::vector<ElectrodeSite>;

struct ProbeIdentity
{
    Locator locator;
    std::string partNumber;
    std::string serialNumber;
};

struct Preset
{
    std::string presetId;
    std::string label;
    ElectrodeMap electrodeMap;
};

enum class ControlMode
{
    IDLE,
    ACQUIRING,
    RECORDING,
    UNKNOWN
};

struct PresetInventory
{
    int processorId = 0;
    std::string probeId;
    ProbeIdentity probe;
    ControlMode mode = ControlMode::UNKNOWN;
    std::string inventoryGeneration;
    std::vector<Preset> presets;
    ElectrodeMap currentMap;
};

struct PresetMutation
{
    std::string presetId;
    std::string expectedInventoryGeneration;
    std::string expectedMapHash;
};

struct MutationDecision
{
    bool allowed = false;
    std::string code;
    std::string targetMapHash;
    const Preset* target = nullptr;
};

std::string stablePresetId (const std::string& probePartNumber,
                            const std::string& label);
std::string stableCommittedStateKey (const ProbeIdentity& probe);
std::string canonicalElectrodeMapHash (const ElectrodeMap& map);
std::string controlModeToString (ControlMode mode);
std::string serializePresetInventory (const PresetInventory& inventory);
std::string serializeAgentSuccess (const std::string& resultObjectJson);
std::string serializeAgentAccepted (const std::string& operationId);
std::string serializePresetInventory (const std::vector<PresetInventory>& inventories,
                                      const std::string& inventoryGeneration);
std::string presetInventoryGeneration (const std::vector<PresetInventory>& inventories);
MutationDecision validatePresetMutation (const PresetInventory& inventory,
                                         const PresetMutation& mutation);
bool isPresetTargetPublishable (ProbeStatus status,
                                bool valid,
                                bool presetFamilySupported,
                                bool disabled);
bool shouldInvalidateCommittedState (ProbeStatus status);

class CommittedPresetStateCache
{
public:
    std::optional<ElectrodeMap> resolve (const std::string& key) const;
    void commit (const std::string& key, const ElectrodeMap& acknowledgedState);
    void observeIdentityLost (const std::string& key);
    void observeIdentity (const ProbeIdentity& probe);
    void retainOnly (const std::vector<std::string>& observedKeys);
    void invalidateConnectionEpoch();

private:
    std::map<std::string, ElectrodeMap> states;
};
}
