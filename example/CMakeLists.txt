# Examples CMakeLists.txt

# Common example properties
function(add_http_parse_example name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE co::http_parse)
    
    # Set output directory
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples"
    )
    
    # Add to examples folder in IDEs
    set_target_properties(${name} PROPERTIES FOLDER "Examples")
    
    # Additional arguments for compiler flags, dependencies, etc.
    if(${ARGC} GREATER 2)
        foreach(arg IN LISTS ARGN)
            if(arg STREQUAL "CXX23")
                if(HAS_CXX23_FEATURES)
                    target_compile_features(${name} PRIVATE cxx_std_23)
                    target_compile_definitions(${name} PRIVATE HTTP_PARSE_CXX23_EXAMPLE=1)
                else()
                    message(STATUS "Skipping ${name} - requires C++23 features")
                    return()
                endif()
            elseif(arg STREQUAL "CXX20")
                target_compile_features(${name} PRIVATE cxx_std_20)
            endif()
        endforeach()
    endif()
    
    message(STATUS "Added example: ${name}")
endfunction()

# Basic examples (C++17 compatible)
add_http_parse_example(basic_http1_parsing basic_http1_parsing.cpp)
add_http_parse_example(http2_connection http2_connection.cpp)
add_http_parse_example(version_detection version_detection.cpp)
add_http_parse_example(incremental_parsing incremental_parsing.cpp)

# C++23 example (only if C++23 features are available)
if(HAS_CXX23_FEATURES AND HTTP_PARSE_ENABLE_CXX23)
    add_http_parse_example(cpp23_features cpp23_features.cpp CXX23)
endif()

# Custom target to run all examples
add_custom_target(run_examples
    COMMENT "Running all HTTP parse examples"
)

# Add individual run targets
set(BASIC_EXAMPLES basic_http1_parsing http2_connection version_detection incremental_parsing)

foreach(example IN LISTS BASIC_EXAMPLES)
    add_custom_target(run_${example}
        COMMAND $<TARGET_FILE:${example}>
        DEPENDS ${example}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/examples
        COMMENT "Running ${example}"
    )
    add_dependencies(run_examples run_${example})
endforeach()

# C++23 example run target
if(TARGET cpp23_features)
    add_custom_target(run_cpp23_features
        COMMAND $<TARGET_FILE:cpp23_features>
        DEPENDS cpp23_features
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/examples
        COMMENT "Running cpp23_features"
    )
    add_dependencies(run_examples run_cpp23_features)
endif()

# Installation of examples (optional)
option(HTTP_PARSE_INSTALL_EXAMPLES "Install example binaries" OFF)

if(HTTP_PARSE_INSTALL_EXAMPLES)
    install(TARGETS ${BASIC_EXAMPLES}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/http_parse_examples
        COMPONENT examples
    )
    
    if(TARGET cpp23_features)
        install(TARGETS cpp23_features
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/http_parse_examples
            COMPONENT examples
        )
    endif()
endif()

# Example data files (if any)
# install(FILES sample_requests.txt
#     DESTINATION ${CMAKE_INSTALL_DATADIR}/http_parse/examples
#     COMPONENT examples
# )