/* method.cpp - Copyright (C) 2011 Red Hat, Inc.
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

#include "method.h"

#include "dbusobject.h"
#include "interface.h"
#include "utils.h"
#include <qmf/SchemaProperty.h>
#include <qmf/Schema.h>
#include <sstream>

extern "C" {
    #include "matahari/logging.h"
    #include "matahari/errors.h"
    #include <libxml/xpath.h>
    #include <dbus/dbus.h>
}

Method::Method(xmlNode *node, const Interface *iface) : interface(iface)
{
    xmlChar *nameStr;
    char *method_name;
    xmlNodePtr subnode;
    nameStr = xmlGetProp(node, (const unsigned char *) "name");
    name = (const char *) nameStr;
    asprintf(&method_name, "%s.%s", interface->name.c_str(), name.c_str());
    int i = 0;
    for (subnode = xmlFirstElementChild(node); subnode; subnode = subnode->next) {
        if (xmlStrEqual(subnode->name, (const unsigned char *) "arg")) {
            addArg(new Arg(subnode, i));
            i++;
        }
    }
    xmlFree(nameStr);
    free(method_name);
}

Method::~Method()
{
    list<Arg *>::iterator it, end = allArgs.end();
    for (it = allArgs.begin(); it != end; it++) {
        delete *it;
    }
    allArgs.clear();
    inArgs.clear();
    outArgs.clear();
}

void Method::addArg(Arg *arg)
{
    if (arg->dir == DIR_IN) {
        inArgs.push_back(arg);
    } else {
        outArgs.push_back(arg);
    }
    allArgs.push_back(arg);
}

list<qpid::types::Variant>
Method::argMapToList(const map<string, qpid::types::Variant> &m) const
{
    list<qpid::types::Variant> l;
    list<Arg *>::const_iterator it, end = inArgs.end();
    qpid::types::Variant v;
    for (it = inArgs.begin(); it != end; it++) {
        if ((*it)->name.size() > 0) {
            v = m.at((*it)->name);
        } else {
            mh_crit("Unknown argument");
            v = qpid::types::Variant();
        }
        l.push_back(v);
    }
    return l;
}

list<qpid::types::Variant> Method::call(const list<qpid::types::Variant> &args, GError **err) const
{
    mh_debug("===== Method call =====\nBus name: %s\nObject path: %s\n"
             "Interface: %s\nMethod name: %s\n",
             interface->dbusObject->bus_name.c_str(),
             interface->dbusObject->object_path.c_str(),
             interface->name.c_str(), name.c_str());

    DBusError dbusErr;
    DBusMessage *message, *reply;

    // Check arg count
    if (inArgs.size() != args.size()) {
        g_set_error(err, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "Wrong number of arguments (expected: %lu, found: %lu)",
                    inArgs.size(), args.size());
        mh_crit("%s", (*err)->message);
        return list<qpid::types::Variant>();
    }

    // Create method call
    message = dbus_message_new_method_call(interface->dbusObject->bus_name.c_str(),
                                           interface->dbusObject->object_path.c_str(),
                                           interface->name.c_str(),
                                           name.c_str());
    if (!message) {
        g_set_error(err, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "Unable to create DBus message");
        mh_crit("%s", (*err)->message);
        return list<qpid::types::Variant>();
    }

    DBusMessageIter iter;
    dbus_message_iter_init_append(message, &iter);

    list<Arg *>::const_iterator arg, endArg = inArgs.end();
    list<qpid::types::Variant>::const_iterator value, endValue = args.end();
    for (arg = inArgs.begin(), value = args.begin();
         arg != endArg, value != endValue;
         arg++, value++) {

        (*arg)->addToMessage(&iter, *value);
    }

    // TODO: Async method call
    dbus_error_init (&dbusErr);
    // Call method
    reply = dbus_connection_send_with_reply_and_block(interface->dbusObject->connection, message, CALL_TIMEOUT, &dbusErr);

    if (!reply) {
        g_set_error(err, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "Unable to call DBus method: %s", dbusErr.message);
        mh_crit("%s", (*err)->message);
        return list<qpid::types::Variant>();
    }

    // Convert return arguments from reply message
    dbus_message_iter_init(reply, &iter);
    int current_type;
    qpid::types::Variant::List list;
    qpid::types::Variant v;
    while ((current_type = dbus_message_iter_get_arg_type(&iter)) != DBUS_TYPE_INVALID) {
        v = dbus_message_iter_to_qpid_variant(current_type, &iter);
        mh_debug("Result: %s", v.asString().c_str());
        list.push_back(v);
        dbus_message_iter_next(&iter);
    }

    dbus_message_unref(message);
    dbus_message_unref(reply);

    mh_debug("=======================\n");
    return list;
}

Signal::Signal(xmlNode *node, const Interface *iface) : interface(iface), schema(NULL)
{
    xmlChar *nameStr;
    char *signal_name;
    xmlNodePtr subnode;
    nameStr = xmlGetProp(node, (const unsigned char *) "name");
    name = (const char *) nameStr;
    asprintf(&signal_name, "%s.%s", interface->name.c_str(), name.c_str());
    int i = 0;
    for (subnode = xmlFirstElementChild(node); subnode; subnode = subnode->next) {
        if (xmlStrEqual(subnode->name, (const unsigned char *) "arg")) {
            args.push_back(new Arg(subnode, i));
            i++;
        }
    }
    xmlFree(nameStr);
    free(signal_name);
}

Signal::~Signal()
{
    list<Arg *>::iterator it, end = args.end();
    for (it = args.begin(); it != end; it++) {
        delete *it;
    }
    args.clear();
    delete schema;
}

Arg::Arg(xmlNode *node, int index)
{
    xmlChar *nameStr, *dirStr, *typeStr, *accessStr;
    nameStr = xmlGetProp(node, (const unsigned char *) "name");
    if (nameStr && xmlStrlen(nameStr) > 0) {
        name = string((char *) nameStr);
    } else {
        stringstream ss;
        ss << index;
        name = ss.str();
    }

    typeStr = xmlGetProp(node, (const unsigned char *) "type");
    type = strdup((const char *) typeStr);

    dir = DIR_IN;
    dirStr = xmlGetProp(node, (const unsigned char *) "direction");
    if (dirStr) {
        if (xmlStrEqual(dirStr, (const unsigned char *) "out")) {
            dir = DIR_OUT;
        }
    }

    access = ACCESS_READ_WRITE;
    accessStr = xmlGetProp(node, (const unsigned char *) "access");
    if (accessStr) {
        if (xmlStrEqual(accessStr, (const unsigned char *) "read")) {
            access = ACCESS_READ_ONLY;
        }
    }

    xmlFree(nameStr);
    xmlFree(dirStr);
    xmlFree(typeStr);
    xmlFree(accessStr);
}

Arg::~Arg()
{
    free(type);
}


qmf::SchemaProperty Arg::toSchemaProperty() const
{
    qmf::SchemaProperty prop(name, dbus_type_to_qmf_type(type[0]));
    prop.setAccess(access == ACCESS_READ_ONLY ? qmf::ACCESS_READ_ONLY : qmf::ACCESS_READ_WRITE);
    prop.setDirection(dir == DIR_IN ? qmf::DIR_IN : qmf::DIR_OUT);
    return prop;
}

bool Arg::addToMessage(DBusMessageIter *iter, const qpid::types::Variant &v) const
{
    return message_add_arg(iter, v, type);
}
