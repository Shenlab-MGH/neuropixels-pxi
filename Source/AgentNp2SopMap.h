#pragma once

#include "AgentPresetControl.h"
#include "NeuropixComponents.h"

namespace neuropix::agent
{
ElectrodeMap buildNp2FourShankSopMap (
    const Array<ElectrodeMetadata>& metadata,
    const std::string& label);
std::vector<Preset> buildNp2FourShankSopMaps (
    const Array<ElectrodeMetadata>& metadata);
}
