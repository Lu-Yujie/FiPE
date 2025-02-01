set(UTILITY_SRC
    graphoperations.cpp
    commandparser.cpp)

add_library(utility SHARED ${UTILITY_SRC})

set_target_properties(utility PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)