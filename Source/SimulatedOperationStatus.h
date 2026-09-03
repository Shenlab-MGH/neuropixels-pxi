#pragma once

#include "API/NeuropixAPI.h"

namespace neuropix::simulation
{
constexpr Neuropixels::NP_ErrorCode initialOperationStatus() noexcept
{
    return Neuropixels::SUCCESS;
}

inline void acknowledgeOperationSuccess (
    Neuropixels::NP_ErrorCode& errorCode) noexcept
{
    errorCode = Neuropixels::SUCCESS;
}
}
