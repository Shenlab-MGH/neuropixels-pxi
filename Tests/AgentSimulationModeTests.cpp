#include "AgentSimulationMode.h"

#include <iostream>
#include <string>

#ifndef TEST_EXPECT_AGENT_SIMULATION
#error TEST_EXPECT_AGENT_SIMULATION must be defined for this test target
#endif

namespace
{
int failures = 0;

void expectTrue (bool condition, const char* name)
{
    if (condition)
        return;
    ++failures;
    std::cerr << "FAIL " << name << "\n";
}
}

int main()
{
    const auto plan = neuropix::agent::simulationBuildPlan();

#if TEST_EXPECT_AGENT_SIMULATION
    expectTrue (plan.enabled, "simulation build is explicitly enabled");
    expectTrue (! plan.scanHardware, "simulation build skips hardware scan");
    expectTrue (! plan.showNoDevicePrompt, "simulation build skips no-device modal");
    expectTrue (! plan.showProbeConfigurationDialog,
                "simulation build skips probe configuration modal");
    expectTrue (plan.supportsPxi, "simulation build supports PXI source");
    expectTrue (! plan.supportsOneBox, "simulation build fails closed for OneBox source");
    expectTrue (plan.supportsHeadlessLifecycle,
                "simulation build supports editor-free lifecycle and preset apply");
    expectTrue (std::string (plan.probePartNumber) == "NP2013",
                "simulation build creates an NP2 four-shank probe");
    expectTrue (plan.probeSerialNumber == 2000024001,
                "simulation build exposes its reserved SIM serial");
    expectTrue (plan.slot == 2 && plan.port == 1 && plan.dock == 1,
                "simulation build has a deterministic locator");
#else
    expectTrue (! plan.enabled, "production build defaults simulation off");
    expectTrue (plan.scanHardware, "production build retains hardware scan");
    expectTrue (plan.showNoDevicePrompt, "production build retains no-device modal");
    expectTrue (plan.showProbeConfigurationDialog,
                "production build retains probe configuration modal");
    expectTrue (! plan.supportsHeadlessLifecycle,
                "production build does not claim editor-free hardware control");
#endif

    if (failures != 0)
        return 1;

    std::cout << "PASS agent simulation build-plan contract\n";
    return 0;
}
