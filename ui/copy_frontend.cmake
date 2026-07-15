if(EXISTS "${SOURCE_FILE}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SOURCE_FILE}" "${DESTINATION_FILE}"
        RESULT_VARIABLE copy_result
    )
    if(NOT copy_result EQUAL 0)
        message(FATAL_ERROR "Could not copy TermiteUI.exe beside the host.")
    endif()
    message(STATUS "Copied the VCL Termite frontend")
else()
    message(STATUS "TermiteUI.exe is not built yet; build the VCL frontend before launching Termite.")
endif()
