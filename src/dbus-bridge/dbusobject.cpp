/* dbusobject.cpp - Copyright (C) 2011 Red Hat, Inc.
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

#include "dbusobject.h"

#include <sstream>

#include <qmf/AgentSession.h>
#include <qmf/Data.h>
#include <qmf/DataAddr.h>
#include <qmf/Schema.h>
#include <qmf/SchemaMethod.h>
#include <qmf/SchemaProperty.h>

extern "C" {
    #include "matahari/logging.h"
    #include "matahari/errors.h"
    #include <libxml/xpath.h>
}

#include "interface.h"
#include "method.h"

#define DBUS_INTROSPECTABLE "org.freedesktop.DBus.Introspectable"
#define DBUS_INTROSPECT "Introspect"

DBusHandlerResult
signalCallback(DBusConnection *connection, DBusMessage *message,
               void *user_data);

DBusObject::DBusObject(DBusConnection *conn, const char *_bus_name,
                       const char *_object_path, bool _listenForSignals,
                       GError **error)
    : listenForSignals(_listenForSignals), bus_name(_bus_name),
      object_path(_object_path), connection(conn), session(NULL)
{
    char *name;
    xmlNodePtr root_element, node;
    DBusError err;
    DBusMessage *message, *reply;
    char *xml = NULL;
    Interface *interface;

    // Call DBus introspection
    dbus_error_init(&err);
    message = dbus_message_new_method_call(bus_name.c_str(), object_path.c_str(),
                                           DBUS_INTROSPECTABLE, DBUS_INTROSPECT);
    reply = dbus_connection_send_with_reply_and_block(conn, message,
                                                      CALL_TIMEOUT, &err);
    if (!reply || dbus_error_is_set(&err)) {
        g_set_error(error, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "Unable to introspect: %s %s (%s)", bus_name.c_str(),
                    object_path.c_str(), err.message);
        mh_crit("%s", (*error)->message);
        return;
    }

    dbus_message_get_args(reply, &err, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
    if (dbus_error_is_set(&err)) {
        g_set_error(error, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "Introspection failed: %s", err.message);
        mh_crit("%s", (*error)->message);
        return;
    }
    asprintf(&name, "%s@%s", _bus_name, _object_path);

    // Parse the introspection
    xmlInitParser();
    xmlDocPtr doc = xmlReadMemory(xml, strlen(xml), "noname.xml", NULL, 0);
    root_element = xmlDocGetRootElement(doc);
    for (node = xmlFirstElementChild(root_element); node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE &&
                xmlStrEqual(node->name, (const unsigned char *) "interface")) {

            interface = new Interface(node, this);
            interfaces.push_back(interface);
        }
    }
    if (interfaces.empty()) {
        g_set_error(error, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "No interface found on object '%s'. Is the path '%s' "
                    "correct?", bus_name.c_str(), object_path.c_str());
        mh_crit("%s", (*error)->message);
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();
//     free(xml); // TODO: why it crashes?
    free(name);
}

DBusObject::~DBusObject()
{
    list<Interface *>::iterator it, end = interfaces.end();
    for (it = interfaces.begin(); it != end; it++) {
        delete *it;
    }
    interfaces.clear();

    delete session;
}

void
DBusObject::addToSchema(qmf::AgentSession &_session, GError **error)
{
    session = new qmf::AgentSession(_session);
    qmf::Schema schema(qmf::SCHEMA_TYPE_DATA, bus_name, object_path);
    list<Interface *>::const_iterator iface, endIface = interfaces.end();
    list<Method *>::const_iterator method, endMethod;
    list<Arg *>::const_iterator arg, endArg;
    list<Signal *>::const_iterator signal, endSignal;

    for (iface = interfaces.begin(); iface != endIface; iface++) {
        endMethod = (*iface)->methods.end();
        for (method = (*iface)->methods.begin(); method != endMethod; method++) {
            qmf::SchemaMethod schemaMethod((*iface)->name + "." + (*method)->name);
            endArg = (*method)->allArgs.end();
            for (arg = (*method)->allArgs.begin(); arg != endArg; arg++) {
                schemaMethod.addArgument((*arg)->toSchemaProperty());
            }
            schema.addMethod(schemaMethod);
        }
        endArg = (*iface)->properties.end();
        for (arg = (*iface)->properties.begin(); arg != endArg; arg++) {
            schema.addProperty((*arg)->toSchemaProperty());
        }

        if (listenForSignals) {
            endSignal = (*iface)->signals.end();
            for (signal = (*iface)->signals.begin(); signal != endSignal; signal++) {
                (*signal)->schema = new qmf::Schema(qmf::SCHEMA_TYPE_EVENT, bus_name + "@" + object_path, (*iface)->name + "." + (*signal)->name);
                endArg = (*signal)->args.end();
                for (arg = (*signal)->args.begin(); arg != endArg; arg++) {
                    (*signal)->schema->addProperty((*arg)->toSchemaProperty());
                }
                session->registerSchema(*((*signal)->schema));
            }

            if ((*iface)->signals.size() > 0) {
                // Register listening for signals
                DBusError err;
                dbus_error_init(&err);
                stringstream rule;
                rule << "type='signal',interface='" << (*iface)->name << "'";
                dbus_bus_add_match(connection, rule.str().c_str(), &err);
                if (dbus_error_is_set(&err)) {
                    g_set_error(error, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                                "Adding handler for receiving signals failed: %s",
                                err.message);
                    mh_crit("%s", (*error)->message);
                    return;
                }
            }
        }
    }
    session->registerSchema(schema);
    qmf::Data data(schema);
    string name = bus_name + "@" + object_path;
    session->addData(data, name, false);

    // Register callback for incoming signals
    if (!dbus_connection_add_filter(connection, signalCallback, (void *) this, NULL)) {
        g_set_error(error, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "Unable to register signal handler");
        mh_crit("%s", (*error)->message);
    }
}

bool DBusObject::signalReceived(DBusMessage *message)
{
    // Find matching Signal object
    const Interface *interface = getInterface(string(dbus_message_get_interface(message)));
    if (!interface) {
        mh_crit("No interface %s found\n", dbus_message_get_interface(message));
        return false;
    }
    const Signal *signal = interface->getSignal(dbus_message_get_member(message));
    if (!signal) {
        mh_crit("No signal %s found on interface %s\n", dbus_message_get_member(message), dbus_message_get_interface(message));
        return false;
    }

    mh_debug("Incoming signal %s on interface %s\n", dbus_message_get_member(message), dbus_message_get_interface(message));

    DBusMessageIter iter;
    qmf::Data event(*(signal->schema));
    int current_type;

    // Convert DBus signal arguments to QMF event properties
    dbus_message_iter_init(message, &iter);
    list<Arg *>::const_iterator it, end = signal->args.end();
    for (it = signal->args.begin(); it != end; it++) {
        current_type = dbus_message_iter_get_arg_type(&iter);
        if (current_type == DBUS_TYPE_INVALID) {
            mh_crit("Number of arguments of signal %s don't match the signature", dbus_message_get_member(message));
            return false;
        }
        qpid::types::Variant v = dbus_message_iter_to_qpid_variant(current_type, &iter);
        event.setProperty((*it)->name, v);
        dbus_message_iter_next(&iter);
    }
    // Raise QMF event
    try {
        session->raiseEvent(event, qmf::SEV_CRIT);
    } catch (const qpid::messaging::ConnectionError& e) {
        mh_err("Connection error sending event to broker. (%s)", e.what());
    } catch (const qpid::types::Exception& e) {
        mh_err("Exception sending event to broker. (%s)", e.what());
    }
    return true;
}

const Interface *DBusObject::getInterface(const string &name) const
{
    list<Interface *>::const_iterator it, end = interfaces.end();
    for (it = interfaces.begin(); it != end; it++) {
        if ((*it)->name.compare(name) == 0) {
            return *it;
        }
    }
    return NULL;
}

DBusHandlerResult
signalCallback(DBusConnection *connection, DBusMessage *message, void *user_data)
{
    DBusObject *obj = (DBusObject *) user_data;
    if (obj->signalReceived(message)) {
        return DBUS_HANDLER_RESULT_HANDLED;
    } else {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
}
