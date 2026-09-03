#include "AgentNp2SopMapProductionAdapter.h"

namespace neuropix::agent
{
ElectrodeMap productionNp2FourShankSopMap (
    const Array<ElectrodeMetadata>& metadata,
    const std::string& label)
{
    return buildNp2FourShankSopMap (metadata, label);
}

std::vector<Preset> productionNp2FourShankSopMaps (
    const Array<ElectrodeMetadata>& metadata)
{
    return buildNp2FourShankSopMaps (metadata);
}
}
