
set(BASE "dbus-bridge")
set(QMF_AGENT "matahari-qmf-${BASE}d")

# QMF daemon
if(WITH-QMF AND WITH-DBUS)
    set(SCHEMA_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/qmf/org/matahariproject/QmfPackage.cpp)
    generate_qmf_schemas(${CMAKE_CURRENT_SOURCE_DIR}/schema.xml ${SCHEMA_SOURCES})
    include_directories(${CMAKE_CURRENT_BINARY_DIR} ${dbus-glib_INCLUDE_DIRS} ${libxml2_INCLUDE_DIRS})

    add_executable(${QMF_AGENT} ${BASE}.cpp dbusobject.cpp method.cpp utils.cpp ${SCHEMA_SOURCES})
    target_link_libraries(${QMF_AGENT} mcommon mcommon_qmf ${dbus-glib_LIBRARIES} ${libxml2_LIBRARIES})

    #create_manpage(${QMF_AGENT} ${AGENT_MAN_SECTION} ${AGENT_MAN_DESC})
    install_manpage(${QMF_AGENT} ${AGENT_MAN_SECTION})
    create_service_scripts(${BASE})

    install(TARGETS ${QMF_AGENT} DESTINATION sbin)

endif(WITH-QMF AND WITH-DBUS)
