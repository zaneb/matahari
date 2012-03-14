set(BASE "rpc")
set(BASE_LIB "m${BASE}")
set(QMF_AGENT "matahari-qmf-${BASE}d")

# QMF daemon
if(WITH-QMF)
if(NOT WIN32)
    set(SCHEMA_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/qmf/org/matahariproject/rpc/QmfPackage.cpp)
    generate_qmf_schemas(${CMAKE_CURRENT_SOURCE_DIR}/schema.xml ${SCHEMA_SOURCES})
    include_directories(${CMAKE_CURRENT_BINARY_DIR})

    add_executable(${QMF_AGENT} ${BASE}-qmf.cpp ${SCHEMA_SOURCES})
    target_link_libraries(${QMF_AGENT} ${BASE_LIB} mcommon_qmf)

    #create_manpage(${QMF_AGENT} ${AGENT_MAN_SECTION} ${AGENT_MAN_DESC})
    install_manpage(${QMF_AGENT} ${AGENT_MAN_SECTION})
    create_service_scripts(${BASE})

    install(TARGETS ${QMF_AGENT} DESTINATION sbin)

    install(CODE "file(MAKE_DIRECTORY \$ENV{DESTDIR}/usr/lib/matahari/plugins)")
endif(NOT WIN32)
endif(WITH-QMF)

