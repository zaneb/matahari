/* dbus-bridge.cpp - Copyright (C) 2011 Red Hat, Inc.
 * Written by Radek Novacek <rnovacek@redhat.com>
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

#include "config.h"

#include "dbusobject.h"
#include "interface.h"
#include "method.h"

#include "matahari/agent.h"
#include "qmf/org/matahariproject/QmfPackage.h"
#include <sstream>

extern "C" {
    #include "matahari/logging.h"
    #include <dbus/dbus.h>
    #include <dbus/dbus-glib-lowlevel.h>
}

class DBusAgent : public MatahariAgent
{
public:
    virtual int setup(qmf::AgentSession session);
    virtual gboolean invoke(qmf::AgentSession session, qmf::AgentEvent event,
                            gpointer user_data);
    DBusConnection *conn;

private:
    qmf::org::matahariproject::PackageDefinition _package;
    qmf::Data _instance;
    static const char DBUS_BRIDGE_NAME[];

    const DBusObject *getDBusObject(const char *bus_name,
                                    const char *object_path,
                                    bool listenToSignals, GError **error);
    map<string, DBusObject *> dbus_objects;
};

const char DBusAgent::DBUS_BRIDGE_NAME[] = "DBusBridge";

int
DBusAgent::setup(qmf::AgentSession session)
{
    DBusError err;
    _package.configure(session);
    _instance = qmf::Data(_package.data_DBusBridge);

    _instance.setProperty("uuid", mh_uuid());
    _instance.setProperty("hostname", mh_hostname());

    session.addData(_instance, DBUS_BRIDGE_NAME);

    dbus_error_init(&err);
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        mh_crit("Unable to connect to system bus: %s", err.message);
        return 1;
    }

    return 0;
}

const DBusObject *
DBusAgent::getDBusObject(const char *bus_name, const char *object_path,
                         bool listenToSignals, GError **error)
{
    string name(string(bus_name) + "@" + object_path);

    DBusObject *obj;
    map<string, DBusObject *>::const_iterator it;
    it = dbus_objects.find(name);
    if (it == dbus_objects.end()) {
        obj = new DBusObject(conn, bus_name, object_path, listenToSignals,
                             error);
        if (*error) {
            return NULL;
        }
        dbus_objects.insert(pair<string, DBusObject *> (name, obj));
    } else {
        obj = it->second;
    }

    return obj;
}

gboolean
DBusAgent::invoke(qmf::AgentSession session, qmf::AgentEvent event,
                  gpointer user_data)
{
    GError *error = NULL;
    const Interface *iface = NULL;
    const Method *method = NULL;
    qpid::types::Variant::List argList, results;
    qpid::types::Variant::Map &args = event.getArguments();
    if (event.getType() != qmf::AGENT_METHOD) {
        return TRUE;
    }

    if (event.getType() == qmf::AGENT_METHOD && event.hasDataAddr()) {
        if (event.getDataAddr().getName() == DBUS_BRIDGE_NAME) {
            if (event.getMethodName() == "call") {

                // Call DBus method using bus name and object path as arguments

                const DBusObject *dbusObject = getDBusObject(
                        args["bus_name"].asString().c_str(),
                        args["object_path"].asString().c_str(), false, &error);
                if (error) {
                    session.raiseException(event, error->message);
                    return TRUE;
                }

                iface = dbusObject->getInterface(args["interface"].asString());
                if (!iface) {
                    session.raiseException(event, "Unknown interface " +
                                           args["interface"].asString());
                    return TRUE;
                }

                method = iface->getMethod(args["method_name"].asString());
                if (!method) {
                    session.raiseException(event, "Unknown method " +
                            args["method"].asString() + " on interface " +
                            args["interface"].asString());
                    return TRUE;
                }

                results = method->call(args["args"].asList(), &error);
                if (error) {
                    session.raiseException(event, error->message);
                    return TRUE;
                }
                event.addReturnArgument("results", results);

            } else if (event.getMethodName() == "add_dbus_object") {

                // Add DBus object to QMF schema and set up signal handling

                DBusObject *dbusObject = new DBusObject(conn,
                        args["bus_name"].asString().c_str(),
                        args["object_path"].asString().c_str(),
                        true, &error);
                if (error) {
                    session.raiseException(event, error->message);
                    return TRUE;
                }

                dbusObject->addToSchema(session, &error);
                if (error) {
                    session.raiseException(event, error->message);
                    return TRUE;
                }
            } else {
                session.raiseException(event,
                                       mh_result_to_str(MH_RES_NOT_IMPLEMENTED));
                goto bail;
            }
        } else {

            // Call method of created DBus object

            string name = event.getDataAddr().getName();
            string methodWithInterface = event.getMethodName().c_str();

            // Parse bus name and object path from DataAddr name of the event
            // Format is: "bus_name@object_path"
            unsigned int pos = name.find('@');
            if (pos == string::npos) {
                mh_crit("Unable to parse bus name and object path from %s "
                        "(should be \"bus_name@object_path\")\n", name.c_str());
                session.raiseException(event,
                                       "Unknown bus name and/or object path");
                return TRUE;
            }
            string bus_name = name.substr(0, pos);
            string object_path = name.substr(pos + 1);

            // Parse interface and method name from requested method name
            // Format is "interface.method", so take what is after last dot
            // as method
            pos = methodWithInterface.rfind(".");
            if (pos == string::npos) {
                mh_crit("Unable to parse interface and method name from %s "
                        "(should be \"interface.method_name\")\n",
                        methodWithInterface.c_str());
                session.raiseException(event, "Unknown interface and/or method");
                return TRUE;
            }
            string ifaceName = methodWithInterface.substr(0, pos);
            string methodName = methodWithInterface.substr(pos + 1);

            mh_debug("Bus name: %s object path: %s interface: %s method: %s\n",
                     bus_name.c_str(), object_path.c_str(),
                     ifaceName.c_str(), methodName.c_str());

            // Now get the dbus object
            const DBusObject *dbusObject = getDBusObject(bus_name.c_str(),
                                                         object_path.c_str(),
                                                         false, &error);
            if (error) {
                session.raiseException(event, error->message);
                return TRUE;
            }

            iface = dbusObject->getInterface(ifaceName);
            if (!iface) {
                session.raiseException(event, "Unknown interface " + ifaceName);
                return TRUE;
            }

            method = iface->getMethod(methodName);
            if (!method) {
                session.raiseException(event, "Unknown method " + methodName +
                                              " on interface " + ifaceName);
                return TRUE;
            }

            // Convert map of arguments to the list of arguments
            argList = method->argMapToList(args);

            // Call the method
            results = method->call(argList, &error);
            if (error) {
                session.raiseException(event, error->message);
                return TRUE;
            }

            // Check number of result
            if (results.size() != method->outArgs.size()) {
                stringstream ss;
                ss << "Method returns different number of return arguments "
                      "(expected: " << method->outArgs.size() <<
                      ", got: " << results.size();
                session.raiseException(event, ss.str());
                return TRUE;
            }

            // Convert the results
            qpid::types::Variant::List::iterator res, resEnd = results.end();
            list<Arg *>::const_iterator arg, argEnd = method->outArgs.end();
            for (res = results.begin(), arg = method->outArgs.begin();
                 res != resEnd && arg != argEnd;
                 res++, arg++) {

                event.addReturnArgument((*arg)->name, *res);
            }
        }
    }

    session.methodSuccess(event);

bail:
    return TRUE;
}

int
main(int argc, char **argv)
{
    DBusAgent agent;
    int rc = agent.init(argc, argv, "DBusBridge");
    if (rc == 0) {
        mainloop_track_children(G_PRIORITY_DEFAULT);
        dbus_connection_setup_with_g_main(agent.conn, NULL);
        agent.run();
    }

    return rc;
}

