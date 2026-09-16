function(init_git_submodules EXTERNAL_DIR SUBMODULE_PATHS)
    set(REPO_DIR "${EXTERNAL_DIR}/../../")
    message(STATUS "Initializing submodules in ${REPO_DIR}")

    foreach(SUBMODULE_PATH ${SUBMODULE_PATHS})
        # Sources are vendored in this fork; only fetch what is missing, so the
        # build also works from a tarball without git metadata.
        set(SUBMODULE_DIR "${SUBMODULE_PATH}")
        if(NOT IS_ABSOLUTE "${SUBMODULE_DIR}")
            set(SUBMODULE_DIR "${REPO_DIR}/${SUBMODULE_PATH}")
        endif()
        file(GLOB SUBMODULE_CONTENT "${SUBMODULE_DIR}/*")
        if(SUBMODULE_CONTENT)
            message(STATUS "Submodule ${SUBMODULE_PATH} present, skipping.")
            continue()
        endif()
        execute_process(
            COMMAND git submodule update --init --recursive ${SUBMODULE_PATH}
            WORKING_DIRECTORY ${REPO_DIR}
            RESULT_VARIABLE INIT_SUBMODULE_RESULT
            OUTPUT_VARIABLE INIT_SUBMODULE_OUTPUT
            ERROR_VARIABLE INIT_SUBMODULE_ERROR
        )

        if(INIT_SUBMODULE_RESULT EQUAL 0)
            message(STATUS "Submodules initialized successfully.")
        else()
            message(FATAL_ERROR "Failed to initialize submodules: ${INIT_SUBMODULES_ERROR}")
        endif()
    endforeach()
endfunction()