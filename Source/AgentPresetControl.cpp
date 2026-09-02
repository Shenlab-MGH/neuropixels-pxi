#include "AgentPresetControl.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <set>
#include <sstream>
#include <tuple>

namespace neuropix::agent
{
namespace
{
std::string escapeJson (const std::string& value)
{
    std::ostringstream output;
    for (const unsigned char character : value)
    {
        switch (character)
        {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20)
                    output << "\\u" << std::hex << std::setw (4)
                           << std::setfill ('0') << static_cast<int> (character)
                           << std::dec;
                else
                    output << character;
        }
    }
    return output.str();
}

std::string canonicalMap (const ElectrodeMap& map)
{
    auto ordered = map;
    std::sort (ordered.begin(), ordered.end(), [] (const auto& left, const auto& right)
    {
        return std::tie (left.channelIndex, left.shankIndex, left.bankIndex, left.electrodeIndex)
               < std::tie (right.channelIndex, right.shankIndex, right.bankIndex, right.electrodeIndex);
    });

    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < ordered.size(); ++index)
    {
        if (index != 0)
            output << ',';
        const auto& site = ordered[index];
        output << '[' << site.channelIndex << ',' << site.shankIndex << ','
               << site.bankIndex << ',' << site.electrodeIndex << ']';
    }
    output << ']';
    return output.str();
}

constexpr std::array<std::uint32_t, 64> sha256Constants {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

std::uint32_t rotateRight (std::uint32_t value, unsigned amount)
{
    return (value >> amount) | (value << (32 - amount));
}

std::string sha256 (const std::string& input)
{
    std::vector<unsigned char> bytes (input.begin(), input.end());
    const std::uint64_t bitLength = static_cast<std::uint64_t> (bytes.size()) * 8;
    bytes.push_back (0x80);
    while ((bytes.size() % 64) != 56)
        bytes.push_back (0);
    for (int shift = 56; shift >= 0; shift -= 8)
        bytes.push_back (static_cast<unsigned char> (bitLength >> shift));

    std::array<std::uint32_t, 8> hash {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    for (std::size_t offset = 0; offset < bytes.size(); offset += 64)
    {
        std::array<std::uint32_t, 64> words {};
        for (std::size_t i = 0; i < 16; ++i)
        {
            const auto base = offset + i * 4;
            words[i] = (static_cast<std::uint32_t> (bytes[base]) << 24)
                     | (static_cast<std::uint32_t> (bytes[base + 1]) << 16)
                     | (static_cast<std::uint32_t> (bytes[base + 2]) << 8)
                     | static_cast<std::uint32_t> (bytes[base + 3]);
        }
        for (std::size_t i = 16; i < words.size(); ++i)
        {
            const auto s0 = rotateRight (words[i - 15], 7) ^ rotateRight (words[i - 15], 18) ^ (words[i - 15] >> 3);
            const auto s1 = rotateRight (words[i - 2], 17) ^ rotateRight (words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        auto a = hash[0], b = hash[1], c = hash[2], d = hash[3];
        auto e = hash[4], f = hash[5], g = hash[6], h = hash[7];
        for (std::size_t i = 0; i < words.size(); ++i)
        {
            const auto s1 = rotateRight (e, 6) ^ rotateRight (e, 11) ^ rotateRight (e, 25);
            const auto choice = (e & f) ^ (~e & g);
            const auto temp1 = h + s1 + choice + sha256Constants[i] + words[i];
            const auto s0 = rotateRight (a, 2) ^ rotateRight (a, 13) ^ rotateRight (a, 22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temp2 = s0 + majority;
            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }
        hash[0] += a; hash[1] += b; hash[2] += c; hash[3] += d;
        hash[4] += e; hash[5] += f; hash[6] += g; hash[7] += h;
    }

    std::ostringstream output;
    for (const auto word : hash)
        output << std::hex << std::setw (8) << std::setfill ('0') << word;
    return output.str();
}
}

std::string stablePresetId (const std::string& probePartNumber,
                            const std::string& label)
{
    std::string result;
    auto append = [&result] (const std::string& value)
    {
        bool separator = false;
        for (const unsigned char character : value)
        {
            if (std::isalnum (character) != 0 || character == '_')
            {
                if (separator && ! result.empty() && result.back() != ':')
                    result.push_back ('-');
                result.push_back (static_cast<char> (std::tolower (character)));
                separator = false;
            }
            else
            {
                separator = true;
            }
        }
        while (! result.empty() && result.back() == '-')
            result.pop_back();
    };

    append (probePartNumber);
    result.push_back (':');
    append (label);
    return result;
}

std::string stableCommittedStateKey (const ProbeIdentity& probe)
{
    return std::to_string (probe.locator.slot) + ":"
           + std::to_string (probe.locator.port) + ":"
           + std::to_string (probe.locator.dock) + ":"
           + probe.serialNumber;
}

std::string canonicalElectrodeMapHash (const ElectrodeMap& map)
{
    for (const auto& site : map)
        if (site.channelIndex < 0 || site.shankIndex < 0
            || site.bankIndex < 0 || site.electrodeIndex < 0)
            return {};
    return "sha256:" + sha256 (canonicalMap (map));
}

std::string controlModeToString (ControlMode mode)
{
    switch (mode)
    {
        case ControlMode::IDLE: return "IDLE";
        case ControlMode::ACQUIRING: return "ACQUIRING";
        case ControlMode::RECORDING: return "RECORDING";
        case ControlMode::UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN";
}

namespace
{
void appendPresetTarget (std::ostringstream& output, const PresetInventory& inventory)
{
    const auto currentHash = canonicalElectrodeMapHash (inventory.currentMap);
    output << "{\"processor_id\":" << inventory.processorId
           << ",\"probe_id\":\"" << escapeJson (inventory.probeId)
           << "\",\"probe_serial\":\"" << escapeJson (inventory.probe.serialNumber)
           << "\",\"slot\":" << inventory.probe.locator.slot
           << ",\"port\":" << inventory.probe.locator.port
           << ",\"dock\":" << inventory.probe.locator.dock
           << ",\"available_presets\":[";

    for (std::size_t index = 0; index < inventory.presets.size(); ++index)
    {
        if (index != 0)
            output << ',';
        const auto& preset = inventory.presets[index];
        output << "{\"preset_id\":\"" << escapeJson (preset.presetId)
               << "\",\"preset_label\":\"" << escapeJson (preset.label)
               << "\",\"electrode_map_hash\":\""
               << canonicalElectrodeMapHash (preset.electrodeMap) << "\"}";
    }
    output << "],\"selected\":{\"selection_kind\":";
    const auto selected = std::find_if (inventory.presets.begin(), inventory.presets.end(),
                                        [&currentHash] (const auto& preset)
                                        { return canonicalElectrodeMapHash (preset.electrodeMap) == currentHash; });
    if (selected == inventory.presets.end())
    {
        output << "\"custom\",\"preset_id\":null,\"preset_label\":null";
    }
    else
    {
        output << "\"preset\",\"preset_id\":\"" << escapeJson (selected->presetId)
               << "\",\"preset_label\":\"" << escapeJson (selected->label) << '"';
    }
    output << ",\"electrode_map_hash\":\"" << currentHash << "\"}}";
}
}

std::string serializePresetInventory (const std::vector<PresetInventory>& inventories,
                                      const std::string& inventoryGeneration)
{
    std::ostringstream output;
    output << "{\"inventory_generation\":\"" << escapeJson (inventoryGeneration)
           << "\",\"targets\":[";
    for (std::size_t index = 0; index < inventories.size(); ++index)
    {
        if (index != 0)
            output << ',';
        appendPresetTarget (output, inventories[index]);
    }
    output << "]}";
    return output.str();
}

std::string serializePresetInventory (const PresetInventory& inventory)
{
    return serializePresetInventory (std::vector<PresetInventory> { inventory },
                                     inventory.inventoryGeneration);
}

std::string serializeAgentSuccess (const std::string& resultObjectJson)
{
    if (resultObjectJson.size() < 2
        || resultObjectJson.front() != '{'
        || resultObjectJson.back() != '}')
        return {};
    return "{\"ok\":true,\"result\":" + resultObjectJson + '}';
}

std::string serializeAgentAccepted (const std::string& operationId)
{
    if (operationId.empty())
        return {};
    return "{\"ok\":true,\"accepted\":true,\"operation_id\":\""
           + escapeJson (operationId) + "\"}";
}

std::string presetInventoryGeneration (const std::vector<PresetInventory>& inventories)
{
    std::ostringstream output;
    output << "oe-preset-inventory-v1";
    for (const auto& value : inventories)
    {
        output << '|' << value.processorId << '|' << value.probeId << '|'
               << value.probe.partNumber << '|' << value.probe.serialNumber;
        for (const auto& preset : value.presets)
            output << '|' << preset.presetId << '|'
                   << canonicalElectrodeMapHash (preset.electrodeMap);
    }
    return output.str();
}

MutationDecision validatePresetMutation (const PresetInventory& inventory,
                                         const PresetMutation& mutation)
{
    if (inventory.mode == ControlMode::UNKNOWN)
        return { false, "mode_unknown", {}, nullptr };
    if (inventory.mode != ControlMode::IDLE)
        return { false, "not_idle", {}, nullptr };
    if (mutation.expectedInventoryGeneration != inventory.inventoryGeneration)
        return { false, "inventory_generation_mismatch", {}, nullptr };
    const auto preset = std::find_if (inventory.presets.begin(), inventory.presets.end(),
                                      [&mutation] (const auto& candidate)
                                      { return candidate.presetId == mutation.presetId; });
    if (preset == inventory.presets.end())
        return { false, "preset_not_supported", {}, nullptr };

    const auto targetMapHash = canonicalElectrodeMapHash (preset->electrodeMap);
    if (mutation.expectedMapHash != targetMapHash)
        return { false, "electrode_map_expectation_mismatch", {}, nullptr };

    return { true, "ok", targetMapHash, &*preset };
}

bool isPresetTargetPublishable (ProbeStatus status,
                                bool valid,
                                bool presetFamilySupported,
                                bool disabled)
{
    const bool connectedState = status == ProbeStatus::CONNECTED
                                || status == ProbeStatus::ACQUIRING
                                || status == ProbeStatus::RECORDING;
    return connectedState && valid && presetFamilySupported && ! disabled;
}

bool shouldInvalidateCommittedState (ProbeStatus status)
{
    return status == ProbeStatus::DISCONNECTED
           || status == ProbeStatus::CONNECTING;
}

std::optional<ElectrodeMap> CommittedPresetStateCache::resolve (
    const std::string& key) const
{
    const auto existing = states.find (key);
    if (existing != states.end())
        return existing->second;
    return std::nullopt;
}

void CommittedPresetStateCache::commit (const std::string& key,
                                        const ElectrodeMap& acknowledgedState)
{
    states[key] = acknowledgedState;
}

void CommittedPresetStateCache::observeIdentityLost (const std::string& key)
{
    states.erase (key);
}

void CommittedPresetStateCache::observeIdentity (const ProbeIdentity& probe)
{
    const auto currentKey = stableCommittedStateKey (probe);
    const auto locatorPrefix = std::to_string (probe.locator.slot) + ":"
                               + std::to_string (probe.locator.port) + ":"
                               + std::to_string (probe.locator.dock) + ":";
    for (auto iterator = states.begin(); iterator != states.end();)
    {
        if (iterator->first != currentKey
            && iterator->first.rfind (locatorPrefix, 0) == 0)
            iterator = states.erase (iterator);
        else
            ++iterator;
    }
}

void CommittedPresetStateCache::retainOnly (
    const std::vector<std::string>& observedKeys)
{
    const std::set<std::string> retained (observedKeys.begin(), observedKeys.end());
    for (auto iterator = states.begin(); iterator != states.end();)
    {
        if (retained.count (iterator->first) == 0)
            iterator = states.erase (iterator);
        else
            ++iterator;
    }
}

void CommittedPresetStateCache::invalidateConnectionEpoch()
{
    states.clear();
}
}
