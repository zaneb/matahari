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

#include "method.h"

DBusHandlerResult
signalCallback(DBusConnection *connection, DBusMessage *message,
               void *user_data);

DBusObject::DBusObject(DBusConnection *conn, const char *_bus_name,
                       const char *_object_path, const char *_interface,
                       bool _listenForSignals, GError **error)
    : listenForSignals(_listenForSignals), session(NULL), connection(conn), bus_name(_bus_name),
      object_path(_object_path), interface(_interface), isRegistered(false)
{
    char *name;
    xmlNodePtr root_element, node, subnode;
    const char *xml = NULL;
    bool interface_found = false;

    xml = dbus_introspect(conn, bus_name, object_path, error);

    if (*error) {
        return;
    } else if (xml == NULL) {
        g_set_error(error, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "Introspection of object '%s' with path '%s' failed",
                    _bus_name, _object_path);
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

            name = (char *) xmlGetProp(node, (const xmlChar *) "name");

            if (strcmp(name, _interface) == 0) {
                interface_found = true;
                for (subnode = xmlFirstElementChild(node); subnode; subnode = subnode->next) {

                    if (xmlStrEqual(subnode->name, (const xmlChar *) "method")) {
                        // Method
                        methods.push_back(new Method(subnode, this));
                    } else if (xmlStrEqual(subnode->name, (const xmlChar *) "property")) {
                        // Property
                        properties.push_back(new Arg(subnode, 0));
                    } else if (xmlStrEqual(subnode->name, (const xmlChar *) "signal")) {
                        // Signal
                        signals.push_back(new Signal(subnode, this));
                    }
                }
                break;
            }
        }
    }
    if (!interface_found) {
        g_set_error(error, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "No such interface '%s' found on object '%s' with path '%s'",
                    interface.c_str(), bus_name.c_str(), object_path.c_str());
        mh_crit("%s", (*error)->message);
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();
    free(name);
}

DBusObject::~DBusObject()
{
    list<Method *>::iterator itMethod, endMethod = methods.end();
    list<Arg *>::iterator itArg, endArg = properties.end();
    list<Signal *>::iterator itSignal, endSignal = signals.end();

    for (itMethod = methods.begin(); itMethod != endMethod; itMethod++) {
        delete *itMethod;
    }
    methods.clear();

    for (itArg = properties.begin(); itArg != endArg; itArg++) {
        delete *itArg;
    }
    properties.clear();

    for (itSignal = signals.begin(); itSignal != endSignal; itSignal++) {
        delete *itSignal;
    }
    signals.clear();

    delete session;
}

void
DBusObject::addToSchema(qmf::AgentSession &_session, GError **error)
{
    if (isRegistered)
        return;

    mh_debug("Register: %s@%s, %s", bus_name.c_str(), object_path.c_str(), interface.c_str());
    session = new qmf::AgentSession(_session);
    qmf::Schema schema(qmf::SCHEMA_TYPE_DATA, bus_name + "@" + object_path, interface);
    list<Method *>::const_iterator method, endMethod = methods.end();
    list<Arg *>::const_iterator arg, endArg;
    list<Signal *>::const_iterator signal, endSignal = signals.end();

    for (method = methods.begin(); method != endMethod; method++) {
        qmf::SchemaMethod schemaMethod((*method)->name);
        endArg = (*method)->allArgs.end();
        for (arg = (*method)->allArgs.begin(); arg != endArg; arg++) {
            schemaMethod.addArgument((*arg)->toSchemaProperty());
        }
        schema.addMethod(schemaMethod);
    }
    // Add properties to schema
    endArg = properties.end();
    for (arg = properties.begin(); arg != endArg; arg++) {
        schema.addProperty((*arg)->toSchemaProperty());
    }

    isRegistered = true;

    if (listenForSignals) {
        endSignal = signals.end();
        for (signal = signals.begin(); signal != endSignal; signal++) {
            (*signal)->schema = new qmf::Schema(qmf::SCHEMA_TYPE_EVENT, bus_name + "@" + object_path, interface + "." + (*signal)->name);
            endArg = (*signal)->args.end();
            for (arg = (*signal)->args.begin(); arg != endArg; arg++) {
                (*signal)->schema->addProperty((*arg)->toSchemaProperty());
            }
            session->registerSchema(*((*signal)->schema));
        }

        if (signals.size() > 0) {
            // Register listening for signals
            DBusError err;
            dbus_error_init(&err);
            stringstream rule;
            rule << "type='signal',interface='" << interface << "'";
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
    session->registerSchema(schema);
    qmf::Data data(schema);
    string name = bus_name + "@" + object_path + "@" + interface;

    // Set values of the properties
    if (properties.size() > 0) {
        qList ifaces;
        ifaces.push_back(interface);
        qList ret = DBusObject::call(connection, bus_name.c_str(),
                                      object_path.c_str(), PROPERTIES_IFACE,
                                      "GetAll", ifaces, error);
        if (*error != NULL) {
            mh_warn("GetAll method failed (%s)", (*error)->message);
        } else if (ret.size() < 1) {
            mh_warn("GetAll method failed");
        } else {
            // First argument is list of properties
            qList props = ret.front().asList();

            qList::iterator it, end = props.end();
            for (it = props.begin(); it != end; it++) {
                // Each item is list of two items: name and value of the property
                qList::iterator prop = (*it).asList().begin();
                if (getProperty((*prop).asString()) != NULL) {
                    mh_debug("Setting value of property: %s", (*prop).asString().c_str());
                    string propName = (*prop).asString();
                    prop++;
                    data.setProperty(propName, *(prop));
                } else {
                    mh_warn("Unknown property: %s", (*prop).asString().c_str());
                }
            }
        }
    }
    session->addData(data, name);

    // Register callback for incoming signals
    if (!dbus_connection_add_filter(connection, signalCallback, (void *) this, NULL)) {
        g_set_error(error, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "Unable to register signal handler");
        mh_crit("%s", (*error)->message);
    }
}

bool
DBusObject::signalReceived(DBusMessage *message) const
{
    // Find matching Signal object
    const Signal *signal = getSignal(dbus_message_get_member(message));
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

const Method *
DBusObject::getMethod(const string &_name) const
{
    list<Method *>::const_iterator it, end = methods.end();
    for (it = methods.begin(); it != end; it++) {
        if ((*it)->name.compare(_name) == 0) {
            return *it;
        }
    }
    return NULL;
}

const Signal *
DBusObject::getSignal(const string &_name) const
{
    list<Signal *>::const_iterator it, end = signals.end();
    for (it = signals.begin(); it != end; it++) {
        if ((*it)->name.compare(_name) == 0) {
            return *it;
        }
    }
    return NULL;
}

const Arg *
DBusObject::getProperty(const string &_name) const
{
    list<Arg *>::const_iterator it, end = properties.end();
    for (it = properties.begin(); it != end; it++) {
        if ((*it)->name.compare(_name) == 0) {
            return *it;
        }
    }
    return NULL;
}

qList
DBusObject::call(DBusConnection *conn,
     const char *bus_name, const char *object_path, const char *interface,
     const char *methodName, const qList &args, GError **err)
{
    DBusObject obj(conn, bus_name, object_path, interface, false, err);
    if (*err) {
        return qList();
    }
    const Method *method = obj.getMethod(methodName);
    if (method == NULL) {
        g_set_error(err, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "No such method '%s' found on object '%s' with path '%s' and interface '%s'",
                    methodName, bus_name, object_path, interface);
        return qList();

    }
    return method->call(args, err);
}

qList
DBusObject::getObjectPaths(DBusConnection *conn, const string &bus_name,
               const string &object_path)
{
    GError *error = NULL;
    qList object_paths;

    const char *xml = dbus_introspect(conn, bus_name, object_path, &error);

    if (error)
        return qList();

    xmlInitParser();
    xmlDocPtr doc = xmlReadMemory(xml, strlen(xml), "noname.xml", NULL, 0);
    xmlNodePtr root_element = xmlDocGetRootElement(doc), node;
    xmlChar *name;
    string path;
    bool hasContent = false;

    for (node = xmlFirstElementChild(root_element); node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (xmlStrEqual(node->name, (const unsigned char *) "node")) {
                name = xmlGetProp(node, (const xmlChar *) "name");
                if (object_path.size() > 1) {
                    path = object_path + "/" + (char *) name;
                } else {
                    path = object_path + (char *) name;
                }
                qList paths = getObjectPaths(conn,
                                                                  bus_name,
                                                                  path);
                object_paths.insert(object_paths.end(), paths.begin(),
                                    paths.end());
                xmlFree(name);
            } else {
                hasContent = true;
            }
        }
    }
    if (hasContent) {
        object_paths.push_back(object_path);
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();

    return object_paths;
}

qList
DBusObject::getInterfaces(DBusConnection *conn, const string &bus_name,
              const string &object_path)
{
    GError *error = NULL;
    qList interfaces;
    xmlChar *name;
    const char *xml = dbus_introspect(conn, bus_name, object_path, &error);

    if (error)
        return qList();

    xmlInitParser();
    xmlDocPtr doc = xmlReadMemory(xml, strlen(xml), "noname.xml", NULL, 0);
    xmlNodePtr root_element = xmlDocGetRootElement(doc), node;

    for (node = xmlFirstElementChild(root_element); node; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (xmlStrEqual(node->name, (const unsigned char *) "interface")) {
                name = xmlGetProp(node, (const xmlChar *) "name");
                interfaces.push_back((char *) name);
                xmlFree(name);
            }
        }
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return interfaces;
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
