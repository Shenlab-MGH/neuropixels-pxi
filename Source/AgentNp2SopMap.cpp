#include "AgentNp2SopMap.h"

#include <map>

namespace neuropix::agent
{
ElectrodeMap buildNp2FourShankSopMap (
    const Array<ElectrodeMetadata>& metadata,
    const std::string& label)
{
    const auto indices = np2FourShankSopElectrodeIndices (label);
    if (indices.empty())
        return {};

    std::map<int, const ElectrodeMetadata*> byGlobalIndex;
    for (const auto& electrode : metadata)
        if (! byGlobalIndex.emplace (electrode.global_index, &electrode).second)
            return {};

    ElectrodeMap result;
    result.reserve (indices.size());
    for (const auto index : indices)
    {
        const auto found = byGlobalIndex.find (index);
        if (found == byGlobalIndex.end())
            return {};
        const auto& electrode = *found->second;
        result.push_back ({ electrode.channel, electrode.shank,
                            static_cast<int> (electrode.bank),
                            electrode.global_index });
    }
    return result;
}

std::vector<Preset> buildNp2FourShankSopMaps (
    const Array<ElectrodeMetadata>& metadata)
{
    std::vector<Preset> result;
    for (const auto& label : np2FourShankSopPresetLabels())
    {
        auto map = buildNp2FourShankSopMap (metadata, label);
        if (map.size() != 384)
            return {};
        result.push_back ({ stablePresetId ("NP2013", label), label,
                            std::move (map) });
    }
    return result;
}
}
