include(FindPkgConfig)

pkg_check_modules(CAPSTONE REQUIRED capstone>=5.0.0)

set(RV_ADDR2LINE "riscv64-linux-gnu-addr2line" CACHE STRING "Prog name of addr2line")

add_executable(nexusrv-dump dump.c misc.c)
add_executable(nexusrv-split split.c misc.c)
add_executable(nexusrv-assemble assemble.c)
add_executable(nexusrv-patch patch.c misc.c)
add_executable(nexusrv-replay replay.cpp linux.cpp vm.cpp objfile.cpp sym.cpp inst.cpp misc.c logger.cpp)

set(UTILS "nexusrv-dump;nexusrv-split;nexusrv-assemble;nexusrv-patch;nexusrv-replay")

foreach (utility ${UTILS})
    target_include_directories(${utility} PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(${utility} libnexus-rv)
    set_target_properties(${utility} PROPERTIES INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endforeach ()

target_compile_definitions(nexusrv-replay PUBLIC "-DDEFAULT_ADDR2LINE=\"${RV_ADDR2LINE}\"")
target_include_directories(nexusrv-replay PUBLIC ${CAPSTONE_INCLUDE_DIRS})
target_link_directories(nexusrv-replay PUBLIC ${CAPSTONE_LIBRARY_DIRS})
target_link_libraries(nexusrv-replay ${CAPSTONE_LIBRARIES} bfd-multiarch)

install(TARGETS ${UTILS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)