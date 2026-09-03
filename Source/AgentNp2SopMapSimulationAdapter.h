#pragma once

#include "AgentNp2SopMap.h"

namespace neuropix::agent
{
ElectrodeMap simulationNp2FourShankSopMap (
    const Array<ElectrodeMetadata>& metadata,
    const std::string& label);
std::vector<Preset> simulationNp2FourShankSopMaps (
    const Array<ElectrodeMetadata>& metadata);
}
