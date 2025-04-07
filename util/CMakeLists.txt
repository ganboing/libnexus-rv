add_executable(nexusrv-dump dump.c print.c misc.c)

target_include_directories(nexusrv-dump PUBLIC "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(nexusrv-dump libnexus-rv)

set_target_properties(nexusrv-dump PROPERTIES INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

install(TARGETS nexusrv-dump
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)