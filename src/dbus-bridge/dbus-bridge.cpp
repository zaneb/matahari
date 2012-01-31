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
#include "method.h"

#include "matahari/agent.h"
#include "qmf/org/matahariproject/QmfPackage.h"
#include <sstream>
#include <set>

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

    virtual ~DBusAgent();
private:
    qmf::org::matahariproject::PackageDefinition _package;
    qmf::Data _instance;
    static const char DBUS_BRIDGE_NAME[];

    DBusObject *getDBusObject(const char *bus_name,
                                    const char *object_path,
                                    const char *interface,
                                    bool listenToSignals, GError **error);
    map<string, DBusObject *> dbus_objects;
};

const char DBusAgent::DBUS_BRIDGE_NAME[] = "DBusBridge";

DBusAgent::~DBusAgent()
{
    map<string, DBusObject *>::iterator it, end = dbus_objects.end();
    for (it = dbus_objects.begin(); it != end; it++) {
        delete it->second;
    }
    dbus_objects.clear();

    dbus_connection_unref(conn);
}

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

DBusObject *
DBusAgent::getDBusObject(const char *bus_name, const char *object_path,
                         const char *interface, bool listenToSignals,
                         GError **error)
{
    string name(string(bus_name) + "@" + object_path + "@" + interface);

    DBusObject *obj;
    map<string, DBusObject *>::const_iterator it;
    it = dbus_objects.find(name);
    if (it == dbus_objects.end()) {
        obj = new DBusObject(conn, bus_name, object_path, interface,
                             listenToSignals, error);
        if (*error) {
            return NULL;
        }
        dbus_objects.insert(pair<string, DBusObject *> (name, obj));
    } else {
        obj = it->second;
    }

    return obj;
}

bool
not_well_known(const qpid::types::Variant &value)
{
    return value.asString().size() > 0 && value.asString()[0] == ':';
}

bool
blacklisted_interfaces(const string &interface)
{
    return interface == "org.freedesktop.DBus.Introspectable" ||
           interface == "org.freedesktop.DBus.Properties";
}

// We need function for comparing strings in qpid::types::Variants and
// it has to be in the same namespace as the class
namespace qpid {
    namespace types {
        bool
        operator<(const qVariant &v1, const qVariant &v2)
        {
            return v1.asString().compare(v2.asString()) < 0;
        }
    }
}

gboolean
DBusAgent::invoke(qmf::AgentSession session, qmf::AgentEvent event,
                  gpointer user_data)
{
    GError *error = NULL;
    const Method *method = NULL;
    qList argList, results;
    qMap &args = event.getArguments();
    if ((event.getType() != qmf::AGENT_METHOD) || !event.hasDataAddr()) {
        return TRUE;
    }

    if (event.getDataAddr().getName() == DBUS_BRIDGE_NAME) {
        if (event.getMethodName() == "call") {

            // Call DBus method using bus name and object path as arguments

            const DBusObject *dbusObject = getDBusObject(
                    args["bus_name"].asString().c_str(),
                    args["object_path"].asString().c_str(),
                    args["interface"].asString().c_str(), false, &error);
            if (error) {
                session.raiseException(event, error->message);
                g_error_free(error);
                return TRUE;
            }

            method = dbusObject->getMethod(args["method_name"].asString());
            if (!method) {
                session.raiseException(event, "Unknown method '" +
                        args["method_name"].asString() + "' on interface '" +
                        args["interface"].asString() + "'");
                return TRUE;
            }

            results = method->call(args["args"].asList(), &error);
            if (error) {
                session.raiseException(event, error->message);
                g_error_free(error);
                return TRUE;
            }
            event.addReturnArgument("results", results);

        } else if (event.getMethodName() == "list_dbus_objects") {
            // Get 'ListNames', which returns a list of all currently running
            // DBus objects (both well known and non-well-known names)
            results = DBusObject::call(conn, "org.freedesktop.DBus", "/",
                                       "org.freedesktop.DBus", "ListNames",
                                       qList(), &error);
            if (error) {
                session.raiseException(event, error->message);
                g_error_free(error);
                return TRUE;
            }
            if (results.size() < 1) {
                session.raiseException(event, "ListNames method failed");
                return TRUE;
            }
            qList names = results.front().asList();
            // Filter out not-well-known names
            if (args["only_well_known"].asBool()) {
                names.remove_if(not_well_known);
            }

            // Get 'ListActivatableNames', which returns a list of all DBus
            // object that runs or can be runned (only well known names)
            results = DBusObject::call(conn, "org.freedesktop.DBus", "/",
                                       "org.freedesktop.DBus",
                                       "ListActivatableNames", qList(), &error);
            if (error) {
                session.raiseException(event, error->message);
                g_error_free(error);
                return TRUE;
            }
            if (results.size() < 1) {
                session.raiseException(event, "ListActivatableNames method failed");
                return TRUE;
            }

            // Merge and sort results of ListNames and ListActivatableNames
            // (remove duplicates too)
            set<qVariant> s(names.begin(), names.end());
            s.insert(results.front().asList().begin(), results.front().asList().end());
            names.assign(s.begin(), s.end());

            event.addReturnArgument("dbus_objects", names);

        } else if (event.getMethodName() == "list_object_paths") {
            qList paths = DBusObject::getObjectPaths(conn,
                    args["bus_name"].asString(), "/");

            event.addReturnArgument("object_paths", paths);

        } else if (event.getMethodName() == "list_interfaces") {
            qList interfaces = DBusObject::getInterfaces(conn,
                    args["bus_name"].asString(),
                    args["object_path"].asString());

            event.addReturnArgument("interfaces", interfaces);

        } else if (event.getMethodName() == "add_dbus_object") {

            // Add DBus object to QMF schema and set up signal handling
            qList interfaces;
            qMap::iterator it = args.find("interface");
            if (it != args.end() && (*it).second.asString().size() > 0) {
                interfaces.push_back((*it).second);
            } else {
                // No interface given, create object for all of them (except
                // Introspect and Properties)
                interfaces = DBusObject::getInterfaces(conn,
                        args["bus_name"].asString(),
                        args["object_path"].asString());
                interfaces.remove_if(blacklisted_interfaces);
            }

            qList::iterator iface, end = interfaces.end();
            for (iface = interfaces.begin(); iface != end; iface++) {
                DBusObject *dbusObject = getDBusObject(
                        args["bus_name"].asString().c_str(),
                        args["object_path"].asString().c_str(),
                        (*iface).asString().c_str(),
                        true, &error);
                if (error) {
                    session.raiseException(event, error->message);
                    g_error_free(error);
                    return TRUE;
                }

                // Create QMF object out of DBus introspection
                dbusObject->addToSchema(session, &error);
                if (error) {
                    session.raiseException(event, error->message);
                    g_error_free(error);
                    return TRUE;
                }
            }
            event.addReturnArgument("result", 0);
        } else {
            session.raiseException(event,
                                   mh_result_to_str(MH_RES_NOT_IMPLEMENTED));
            goto bail;
        }
    } else {

        // Call method of created DBus object

        string name = event.getDataAddr().getName();
        string methodName = event.getMethodName().c_str();

        // Parse bus name and object path from DataAddr name of the event
        // Format is: "bus_name@object_path@interface"
        unsigned int pos = name.find('@');
        if (pos == string::npos) {
            mh_crit("Unable to parse bus name, object path and interface from %s "
                    "(should be \"bus_name@object_path@interface\")\n", name.c_str());
            session.raiseException(event,
                                   "Unknown bus name, object path or interface");
            return TRUE;
        }
        string bus_name = name.substr(0, pos);
        string objectAndInterface = name.substr(pos + 1);

        pos = objectAndInterface.find('@');
        if (pos == string::npos) {
            mh_crit("Unable to parse bus name, object path and interface from %s "
                    "(should be \"bus_name@object_path@interface\")\n", name.c_str());
            session.raiseException(event,
                                   "Unknown bus name, object path or interface");
            return TRUE;
        }
        string object_path = objectAndInterface.substr(0, pos);
        string interface = objectAndInterface.substr(pos + 1);

        mh_debug("Bus name: %s object path: %s interface: %s method: %s\n",
                 bus_name.c_str(), object_path.c_str(),
                 interface.c_str(), methodName.c_str());

        // Now get the dbus object
        const DBusObject *dbusObject = getDBusObject(bus_name.c_str(),
                                                     object_path.c_str(),
                                                     interface.c_str(),
                                                     false, &error);
        if (error) {
            session.raiseException(event, error->message);
            g_error_free(error);
            return TRUE;
        }

        method = dbusObject->getMethod(methodName);
        if (!method) {
            session.raiseException(event, "Unknown method " + methodName +
                                          " on interface " + interface);
            return TRUE;
        }

        // Convert map of arguments to the list of arguments
        argList = method->argMapToList(args);

        // Call the method
        results = method->call(argList, &error);
        if (error) {
            session.raiseException(event, error->message);
            g_error_free(error);
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
        qList::iterator res, resEnd = results.end();
        list<Arg *>::const_iterator arg, argEnd = method->outArgs.end();
        for (res = results.begin(), arg = method->outArgs.begin();
                res != resEnd && arg != argEnd;
                res++, arg++) {

            event.addReturnArgument((*arg)->name, *res);
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

