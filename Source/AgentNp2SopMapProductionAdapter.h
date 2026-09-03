#pragma once

#include "AgentNp2SopMap.h"

namespace neuropix::agent
{
ElectrodeMap productionNp2FourShankSopMap (
    const Array<ElectrodeMetadata>& metadata,
    const std::string& label);
std::vector<Preset> productionNp2FourShankSopMaps (
    const Array<ElectrodeMetadata>& metadata);
}
