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

add_executable(neuropix_agent_simulation_off_tests
    AgentSimulationModeTests.cpp
)
target_compile_features(neuropix_agent_simulation_off_tests PRIVATE cxx_std_17)
target_compile_definitions(neuropix_agent_simulation_off_tests PRIVATE
    TEST_EXPECT_AGENT_SIMULATION=0
)
target_include_directories(neuropix_agent_simulation_off_tests PRIVATE ../Source)
add_test(NAME neuropix_agent_simulation_default_off
         COMMAND neuropix_agent_simulation_off_tests)

add_executable(neuropix_agent_simulation_on_tests
    AgentSimulationModeTests.cpp
)
target_compile_features(neuropix_agent_simulation_on_tests PRIVATE cxx_std_17)
target_compile_definitions(neuropix_agent_simulation_on_tests PRIVATE
    OE_AGENT_NEUROPIXELS_SIMULATION=1
    TEST_EXPECT_AGENT_SIMULATION=1
)
target_include_directories(neuropix_agent_simulation_on_tests PRIVATE ../Source)
add_test(NAME neuropix_agent_simulation_on_no_gui_np2_4
         COMMAND neuropix_agent_simulation_on_tests)
