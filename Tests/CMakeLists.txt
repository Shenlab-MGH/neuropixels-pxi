cmake_minimum_required(VERSION 3.15)

project(NeuropixAgentInventoryTests LANGUAGES CXX)

enable_testing()

add_executable(neuropix_agent_inventory_tests
    AgentInventoryTests.cpp
    ../Source/AgentInventory.cpp
)

target_compile_features(neuropix_agent_inventory_tests PRIVATE cxx_std_17)
target_include_directories(neuropix_agent_inventory_tests PRIVATE ../Source)

add_test(NAME neuropix_agent_inventory_contract
         COMMAND neuropix_agent_inventory_tests)

add_executable(neuropix_agent_preset_control_tests
    AgentPresetControlTests.cpp
    ../Source/AgentPresetControl.cpp
)

target_compile_features(neuropix_agent_preset_control_tests PRIVATE cxx_std_17)
target_include_directories(neuropix_agent_preset_control_tests PRIVATE ../Source)

add_test(NAME neuropix_agent_preset_control_contract
         COMMAND neuropix_agent_preset_control_tests)

add_executable(neuropix_simulated_operation_status_tests
    SimulatedOperationStatusTests.cpp
)

target_compile_features(neuropix_simulated_operation_status_tests PRIVATE cxx_std_17)
target_include_directories(neuropix_simulated_operation_status_tests PRIVATE ../Source)

add_test(NAME neuropix_simulated_operation_status
         COMMAND neuropix_simulated_operation_status_tests)
