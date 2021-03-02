cmake_minimum_required(VERSION 2.8)

project(third_party)

file(GLOB SCRIPT_LIST "${CMAKE_CURRENT_SOURCE_DIR}/*.sh")

if(DEFINED ADD_THIRD_PARTY)
    if(${ADD_THIRD_PARTY} STREQUAL "ON")
        foreach(script ${SCRIPT_LIST})
            execute_process(
                COMMAND sh ${script}
                )
        endforeach()
    endif()
endif()
