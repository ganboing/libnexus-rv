add_library(libnexus-rv decoder.c)

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