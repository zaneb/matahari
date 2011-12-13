/* rpc-qmf.cpp - Copyright (C) 2011 Red Hat, Inc.
 * Written by Zane Bitter <zbitter@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file
 * \brief RPC QMF Agent
 */

#include "config.h"

#include <set>
#include "matahari/agent.h"
#include "matahari/object.h"
#include <qmf/Data.h>
#include <qmf/Schema.h>
#include <qmf/SchemaMethod.h>
#include <qmf/SchemaProperty.h>
#include "qmf/org/matahariproject/rpc/QmfPackage.h"

#include "matahari/rpc.h"
#include "matahari/logging.h"
#include "matahari/errors.h"


#define PLUGIN_PACKAGE "org.matahariproject.rpc.plugin"


class RPCAgent;

class RPCPlugin: public MatahariObject
{
public:
    RPCPlugin(RPCAgent& agent, ObjectManager& manager,
              qmf::Schema schema, mh_rpc_plugin_t& plugin);
    ~RPCPlugin();

    bool invoke(qmf::AgentSession& session, qmf::AgentEvent& event);

private:
    typedef std::set<std::string> CommandList;

    RPCAgent& _agent;
    ObjectManager& _manager;
    mh_rpc_plugin_t _plugin;
    CommandList _commands;
};


class RPCAgent : public MatahariAgent
{
public:
    virtual ~RPCAgent();

    virtual int setup(qmf::AgentSession session);
    virtual gboolean invoke(qmf::AgentSession session, qmf::AgentEvent event,
                            gpointer user_data);

    qmf::org::matahariproject::rpc::PackageDefinition _package;

private:
    ObjectManager *_objectManager;

    typedef std::map<std::string, RPCPlugin*> PluginMap;
    PluginMap _plugins;

    void addPlugin(qmf::AgentSession session, mh_rpc_plugin_t& plugin);
};


int
main(int argc, char **argv)
{
    mh_rpc_init(1, &argv[0]);

    RPCAgent *agent = new RPCAgent();
    int rc = agent->init(argc, argv, "rpc");
    if (rc == 0) {
        agent->run();
    }

    mh_rpc_deinit();
    return rc;
}

gboolean
RPCAgent::invoke(qmf::AgentSession session, qmf::AgentEvent event,
                   gpointer user_data)
{
    if (!_objectManager->invoke(session, event)) {
        session.raiseException(event, mh_result_to_str(MH_RES_NOT_IMPLEMENTED));
    }

    return TRUE;
}

void RPCAgent::addPlugin(qmf::AgentSession session, mh_rpc_plugin_t& plugin)
{
    qmf::Schema schema(qmf::SCHEMA_TYPE_DATA, PLUGIN_PACKAGE, plugin.name);

    char **procs = mh_rpc_get_procedures(&plugin);
    if (procs) {
        for (char **p = procs; *p; p++) {
            qmf::SchemaMethod method(*p);

            qmf::SchemaProperty args("args", qmf::SCHEMA_DATA_LIST);
            args.setDirection(qmf::DIR_IN);
            method.addArgument(args);

            qmf::SchemaProperty kwargs("kwargs", qmf::SCHEMA_DATA_MAP);
            kwargs.setDirection(qmf::DIR_IN);
            method.addArgument(kwargs);

            qmf::SchemaProperty result("result", qmf::SCHEMA_DATA_STRING);
            result.setDirection(qmf::DIR_OUT);
            method.addArgument(result);

            schema.addMethod(method);

            free(*p);
        }
        free(procs);
    }

    session.registerSchema(schema);

    _plugins[std::string(plugin.name)] = new RPCPlugin(*this, *_objectManager,
                                                       schema, plugin);
}

int
RPCAgent::setup(qmf::AgentSession session)
{
    size_t num_plugins = 0;
    mh_rpc_plugin_t *plugins = NULL;

    _package.configure(session);
    _objectManager = new ObjectManager(session);

    if (mh_rpc_load_plugins(&num_plugins, &plugins)) {
        return 1;
    }

    for (size_t i = 0; i < num_plugins; i++) {
        addPlugin(session, plugins[i]);
    }
    return 0;
}

RPCAgent::~RPCAgent()
{
    for (PluginMap::iterator p = _plugins.begin(); p != _plugins.end(); p++) {
        delete p->second;
    }
    delete _objectManager;
}


RPCPlugin::RPCPlugin(RPCAgent& agent, ObjectManager& manager,
                     qmf::Schema schema,
                     mh_rpc_plugin_t& plugin):
    MatahariObject(schema),
    _agent(agent),
    _manager(manager),
    _plugin(plugin)
{
    mh_info("Creating Plugin %s", plugin.name);

    uint32_t numMethods = schema.getMethodCount();
    for (uint32_t m = 0; m < numMethods; m++) {
        _commands.insert(schema.getMethod(m).getName());
    }

    _manager += this;
}

RPCPlugin::~RPCPlugin()
{
    _manager -= this;
}

bool
RPCPlugin::invoke(qmf::AgentSession& session, qmf::AgentEvent& event)
{
    if (event.getType() != qmf::AGENT_METHOD) {
        return true;
    }

    const std::string& methodName(event.getMethodName());
    qpid::types::Variant::Map& args = event.getArguments();

    if (_commands.find(methodName) == _commands.end()) {
        return false;
    }

    mh_rpc_t call;
    mh_rpc_call_create(&call, methodName.c_str());

    qpid::types::Variant::List arglist(args["args"].asList());
    for (qpid::types::Variant::List::iterator iter = arglist.begin();
         iter != arglist.end();
         iter++) {
        mh_rpc_call_add_arg(call, iter->asString().c_str());
    }
    qpid::types::Variant::Map argmap(args["kwargs"].asMap());
    for (qpid::types::Variant::Map::iterator iter = argmap.begin();
         iter != argmap.end();
         iter++) {
        mh_rpc_call_add_kw_arg(call,
                               iter->first.c_str(),
                               iter->second.asString().c_str());
    }
    mh_rpc_result_t output;
    enum mh_result result = mh_rpc_invoke(&_plugin, call, &output);

    mh_rpc_call_destroy(call);

    if (result != MH_RES_SUCCESS) {
        session.raiseException(event, mh_result_to_str(result));
        return true;
    }

    if (output.result) {
        event.addReturnArgument("result", output.result);
        free(output.result);

        session.methodSuccess(event);
    } else {
        qmf::Data exception(_agent._package.data_PluginException);
        exception.setProperty("type", output.exc_type);
        free(output.exc_type);
        exception.setProperty("value", output.exc_value);
        free(output.exc_value);
        exception.setProperty("traceback", output.exc_traceback);
        free(output.exc_traceback);

        session.raiseException(event, exception);
    }

    return true;
}

