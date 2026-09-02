if (NOT DEFINED PLUGIN_DLL OR NOT EXISTS "${PLUGIN_DLL}")
    message(FATAL_ERROR "Plugin DLL does not exist: ${PLUGIN_DLL}")
endif()

if (NOT DEFINED DUMPBIN_EXECUTABLE OR NOT EXISTS "${DUMPBIN_EXECUTABLE}")
    message(FATAL_ERROR "dumpbin executable does not exist: ${DUMPBIN_EXECUTABLE}")
endif()

execute_process(
    COMMAND "${DUMPBIN_EXECUTABLE}" /nologo /exports "${PLUGIN_DLL}"
    RESULT_VARIABLE dumpbin_result
    OUTPUT_VARIABLE dumpbin_output
    ERROR_VARIABLE dumpbin_error
)

if (NOT dumpbin_result EQUAL 0)
    message(FATAL_ERROR
        "dumpbin failed for ${PLUGIN_DLL}: ${dumpbin_error}")
endif()

foreach (required_export IN ITEMS getLibInfo getPluginInfo)
    string(REGEX MATCHALL
        "(^|[\r\n])[^\r\n]*[ \t]${required_export}([\r\n]|$)"
        export_rows
        "${dumpbin_output}")
    list(LENGTH export_rows export_count)
    if (NOT export_count EQUAL 1)
        message(FATAL_ERROR
            "Expected exactly one undecorated ${required_export} export in ${PLUGIN_DLL}; found ${export_count}.\n${dumpbin_output}")
    endif()
endforeach()

message(STATUS
    "Verified getLibInfo and getPluginInfo exports in ${PLUGIN_DLL}")
