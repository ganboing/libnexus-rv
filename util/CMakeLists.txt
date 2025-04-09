add_executable(nexusrv-dump dump.c misc.c)
add_executable(nexusrv-assemble assemble.c)

target_include_directories(nexusrv-dump PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(nexusrv-dump libnexus-rv)
set_target_properties(nexusrv-dump PROPERTIES INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

target_include_directories(nexusrv-assemble PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(nexusrv-assemble libnexus-rv)
set_target_properties(nexusrv-assemble PROPERTIES INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

install(TARGETS nexusrv-dump nexusrv-assemble
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)