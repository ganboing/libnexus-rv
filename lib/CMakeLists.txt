add_library(libnexus-rv
        msg-decoder.c
        msg-encoder.c
        msg-printer.c
        msg-reader.c
        trace-decoder.c
        hist-array.cpp
        misc.c )

target_compile_options(libnexus-rv PUBLIC -Wdisabled-optimization -foptimize-sibling-calls)

target_include_directories(libnexus-rv
        PUBLIC "${PROJECT_SOURCE_DIR}/include")

set_target_properties(libnexus-rv PROPERTIES
        PREFIX ""
        VERSION 0.0.1
        SOVERSION 0
)

install(TARGETS libnexus-rv
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})