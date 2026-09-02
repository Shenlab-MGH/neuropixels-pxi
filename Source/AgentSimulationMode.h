#pragma once

#ifndef OE_AGENT_NEUROPIXELS_SIMULATION
#define OE_AGENT_NEUROPIXELS_SIMULATION 0
#endif

namespace neuropix::agent
{
struct SimulationBuildPlan
{
    bool enabled;
    bool scanHardware;
    bool showNoDevicePrompt;
    bool showProbeConfigurationDialog;
    bool supportsPxi;
    bool supportsOneBox;
    const char* probePartNumber;
    int probeSerialNumber;
    int slot;
    int port;
    int dock;
};

constexpr SimulationBuildPlan simulationBuildPlan() noexcept
{
#if OE_AGENT_NEUROPIXELS_SIMULATION
    return { true, false, false, false, true, false,
             "NP2013", 2000024001, 2, 1, 1 };
#else
    return { false, true, true, true, true, true,
             nullptr, 0, 0, 0, 0 };
#endif
}
}
