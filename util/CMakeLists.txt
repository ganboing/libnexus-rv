add_executable(nexusrv-dump dump.c misc.c)
add_executable(nexusrv-split split.c misc.c)
add_executable(nexusrv-assemble assemble.c)
add_executable(nexusrv-replay replay.c misc.c)

set(UTILS "nexusrv-dump;nexusrv-split;nexusrv-assemble;nexusrv-replay")

foreach (utility ${UTILS})
    target_include_directories(${utility} PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(${utility} libnexus-rv)
    set_target_properties(${utility} PROPERTIES INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endforeach ()

install(TARGETS ${UTILS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)