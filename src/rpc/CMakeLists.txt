set(BASE "rpc")
set(BASE_LIB "m${BASE}")
set(QMF_AGENT "matahari-qmf-${BASE}d")

# QMF daemon
if(WITH-QMF)
    set(SCHEMA_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/qmf/org/matahariproject/rpc/QmfPackage.cpp)
    generate_qmf_schemas(${CMAKE_CURRENT_SOURCE_DIR}/schema.xml ${SCHEMA_SOURCES})
    include_directories(${CMAKE_CURRENT_BINARY_DIR})

    add_executable(${QMF_AGENT} ${BASE}-qmf.cpp ${SCHEMA_SOURCES})
    target_link_libraries(${QMF_AGENT} ${BASE_LIB} mcommon_qmf)

    create_manpage(${QMF_AGENT} ${AGENT_MAN_SECTION} ${AGENT_MAN_DESC})
    create_service_scripts(${BASE})

    install(TARGETS ${QMF_AGENT} DESTINATION sbin)

endif(WITH-QMF)

