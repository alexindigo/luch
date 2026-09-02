# Verification plugins — built but never installed. Each exercises one
# contract: socket-probe proves the offline sandbox; delay-unwrap
# proves the async contract (picker before slices, late patchSlice).
add_library(test-plugin-socket-probe MODULE
    plugins/socket-probe/socketprobeplugin.cpp
    plugins/socket-probe/socketprobeplugin.h
)
set_target_properties(test-plugin-socket-probe PROPERTIES
    PREFIX "" OUTPUT_NAME "socket-probe")
target_include_directories(test-plugin-socket-probe PRIVATE
    ${CMAKE_SOURCE_DIR}/src/pluginapi)
target_link_libraries(test-plugin-socket-probe PRIVATE Qt6::Core)

add_library(test-plugin-delay-unwrap MODULE
    plugins/delay-unwrap/delayunwrapplugin.cpp
    plugins/delay-unwrap/delayunwrapplugin.h
)
set_target_properties(test-plugin-delay-unwrap PROPERTIES
    PREFIX "" OUTPUT_NAME "delay-unwrap")
target_include_directories(test-plugin-delay-unwrap PRIVATE
    ${CMAKE_SOURCE_DIR}/src/pluginapi)
target_link_libraries(test-plugin-delay-unwrap PRIVATE Qt6::Core)

add_library(test-plugin-verdict-detect MODULE
    plugins/verdict-detect/verdictdetectplugin.cpp
    plugins/verdict-detect/verdictdetectplugin.h
)
set_target_properties(test-plugin-verdict-detect PROPERTIES
    PREFIX "" OUTPUT_NAME "verdict-detect")
target_include_directories(test-plugin-verdict-detect PRIVATE
    ${CMAKE_SOURCE_DIR}/src/pluginapi)
target_link_libraries(test-plugin-verdict-detect PRIVATE Qt6::Core)
