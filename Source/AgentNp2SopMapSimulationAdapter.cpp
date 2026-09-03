#include "AgentNp2SopMapSimulationAdapter.h"

namespace neuropix::agent
{
ElectrodeMap simulationNp2FourShankSopMap (
    const Array<ElectrodeMetadata>& metadata,
    const std::string& label)
{
    return buildNp2FourShankSopMap (metadata, label);
}

std::vector<Preset> simulationNp2FourShankSopMaps (
    const Array<ElectrodeMetadata>& metadata)
{
    return buildNp2FourShankSopMaps (metadata);
}
}
