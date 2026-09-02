#include "SimulatedOperationStatus.h"

#include <iostream>

int main()
{
    auto errorCode = neuropix::simulation::initialOperationStatus();
    if (errorCode != Neuropixels::SUCCESS)
    {
        std::cerr << "FAIL simulated operation status starts at SUCCESS\n";
        return 1;
    }

    errorCode = Neuropixels::NOTSUPPORTED;
    neuropix::simulation::acknowledgeOperationSuccess (errorCode);
    if (errorCode != Neuropixels::SUCCESS)
    {
        std::cerr << "FAIL simulated successful operation clears prior failure\n";
        return 1;
    }

    std::cout << "PASS 2 simulated operation status checks\n";
    return 0;
}
