#pragma once

#include "AgentInventory.h"

#include <cstdint>
#include <map>
#include <mutex>
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

struct PresetSourceBinding
{
    std::string probeId;
    std::string probeSerial;
    std::size_t sourceIndex;
};

struct SourceResolution
{
    std::optional<std::size_t> sourceIndex;
    std::string code;
};

class PresetHardwareOperationGate
{
    enum class Operation { IDLE, APPLY, REFRESH };

public:
    class Lease
    {
    public:
        Lease (Lease&& other) noexcept;
        Lease& operator= (Lease&&) = delete;
        Lease (const Lease&) = delete;
        Lease& operator= (const Lease&) = delete;
        ~Lease();

    private:
        friend class PresetHardwareOperationGate;
        Lease (PresetHardwareOperationGate& owner, Operation ownedOperation);
        PresetHardwareOperationGate* gate;
        Operation operation;
    };

    bool tryBeginApply();
    bool tryBeginRefresh();
    std::optional<Lease> tryAcquireRefresh();
    void finishApply();
    void finishRefresh();
    bool applyInProgress() const;
    bool refreshInProgress() const;

private:
    mutable std::mutex mutex;
    Operation operation = Operation::IDLE;
};

std::string stablePresetId (const std::string& probePartNumber,
                            const std::string& label);
std::string stableCommittedStateKey (const ProbeIdentity& probe);
std::string canonicalElectrodeMapHash (const ElectrodeMap& map);
std::string controlModeToString (ControlMode mode);
ControlMode controlModeFromCoreState (bool acquisitionActive,
                                      bool recordingActive,
                                      ProbeStatus connectionStatus);
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
SourceResolution resolvePresetSource (
    const std::vector<PresetSourceBinding>& bindings,
    const std::string& probeId,
    const std::string& expectedSerial);

class CommittedPresetStateCache
{
public:
    std::optional<ElectrodeMap> resolve (const std::string& key) const;
    std::optional<ElectrodeMap> resolveForPublication (
        const std::string& key,
        ProbeStatus status,
        bool valid,
        bool presetFamilySupported,
        bool disabled) const;
    std::uint64_t connectionEpoch() const;
    bool commitIfEpoch (const std::string& key,
                        const ElectrodeMap& acknowledgedState,
                        std::uint64_t expectedEpoch);
    void commit (const std::string& key, const ElectrodeMap& acknowledgedState);
    void observeIdentityLost (const std::string& key);
    void observeIdentity (const ProbeIdentity& probe);
    void retainOnly (const std::vector<std::string>& observedKeys);
    void invalidateConnectionEpoch();

private:
    std::map<std::string, ElectrodeMap> states;
    std::uint64_t epoch = 0;
};
}
