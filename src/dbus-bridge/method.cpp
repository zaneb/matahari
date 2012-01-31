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
#include "utils.h"
#include <qmf/SchemaProperty.h>
#include <qmf/Schema.h>
#include <sstream>

extern "C" {
    #include "matahari/logging.h"
    #include "matahari/errors.h"
    #include <libxml/xpath.h>
}

/**
 * Add \p value to DBus message.
 *
 * \param[in] iter iterator for arguments of DBus message
 * \param[in] value value to be added
 * \param[in] signature signature of the message
 *
 * \retval TRUE if adding argument succeeds
 * \retval FALSE otherwise
 */
bool
message_add_arg(DBusMessageIter *iter, qVariant value,
                const char *signature);

Method::Method(xmlNode *node, const DBusObject *obj) : dbusObject(obj)
{
    xmlChar *nameStr;
    xmlNodePtr subnode;
    nameStr = xmlGetProp(node, (const unsigned char *) "name");
    name = (char *) nameStr;
    int i = 0;
    for (subnode = xmlFirstElementChild(node);
         subnode;
         subnode = subnode->next) {

        if (xmlStrEqual(subnode->name, (const unsigned char *) "arg")) {
            addArg(new Arg(subnode, i));
            i++;
        }
    }
    xmlFree(nameStr);
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

qList
Method::argMapToList(const qMap &m) const
{
    qList l;
    list<Arg *>::const_iterator it, end = inArgs.end();
    qVariant v;
    for (it = inArgs.begin(); it != end; it++) {
        if ((*it)->name.size() > 0) {
            v = m.at((*it)->name);
        } else {
            mh_crit("Unknown argument");
            v = qVariant();
        }
        l.push_back(v);
    }
    return l;
}

qList
Method::call(const qList &args, GError **err) const
{
    mh_debug("===== Method call =====\nBus name: %s\nObject path: %s\n"
             "Interface: %s\nMethod name: %s\n",
             dbusObject->bus_name.c_str(),
             dbusObject->object_path.c_str(),
             dbusObject->interface.c_str(), name.c_str());

    DBusError dbusErr;
    DBusMessage *message, *reply;

    // Check arg count
    if (inArgs.size() != args.size()) {
        g_set_error(err, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "Wrong number of arguments (expected: %lu, found: %lu)",
                    inArgs.size(), args.size());
        mh_crit("%s", (*err)->message);
        return qList();
    }

    // Create method call
    message = dbus_message_new_method_call(dbusObject->bus_name.c_str(),
                                           dbusObject->object_path.c_str(),
                                           dbusObject->interface.c_str(),
                                           name.c_str());
    if (!message) {
        g_set_error(err, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "Unable to create DBus message");
        mh_crit("%s", (*err)->message);
        return qList();
    }

    DBusMessageIter iter;
    dbus_message_iter_init_append(message, &iter);

    list<Arg *>::const_iterator arg, endArg = inArgs.end();
    qList::const_iterator value, endValue = args.end();
    for (arg = inArgs.begin(), value = args.begin();
         arg != endArg, value != endValue;
         arg++, value++) {

        (*arg)->addToMessage(&iter, *value);
    }

    dbus_error_init (&dbusErr);
    // Call method
    reply = dbus_connection_send_with_reply_and_block(dbusObject->connection,
                                                      message, CALL_TIMEOUT,
                                                      &dbusErr);

    if (!reply) {
        g_set_error(err, MATAHARI_ERROR, MH_RES_BACKEND_ERROR,
                    "Unable to call DBus method: %s", dbusErr.message);
        mh_crit("%s", (*err)->message);
        return qList();
    }

    // Convert return arguments from reply message
    dbus_message_iter_init(reply, &iter);
    int current_type;
    qList list;
    qVariant v;
    while ((current_type = dbus_message_iter_get_arg_type(&iter))
            != DBUS_TYPE_INVALID) {
        v = dbus_message_iter_to_qpid_variant(current_type, &iter);
        mh_debug("Result: %s", v.asString().c_str());
        list.push_back(v);
        dbus_message_iter_next(&iter);
    }

    dbus_message_unref(message);
    dbus_message_unref(reply);

    mh_debug("=======================");
    return list;
}

Signal::Signal(xmlNode *node, const DBusObject *obj)
: dbusObject(obj), schema(NULL)
{
    xmlChar *nameStr;
    xmlNodePtr subnode;
    nameStr = xmlGetProp(node, (const unsigned char *) "name");
    name = (const char *) nameStr;
    int i = 0;
    for (subnode = xmlFirstElementChild(node);
         subnode;
         subnode = subnode->next) {

        if (xmlStrEqual(subnode->name, (const unsigned char *) "arg")) {
            args.push_back(new Arg(subnode, i));
            i++;
        }
    }
    xmlFree(nameStr);
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
    prop.setAccess(access == ACCESS_READ_ONLY ?
                   qmf::ACCESS_READ_ONLY :
                   qmf::ACCESS_READ_WRITE);
    prop.setDirection(dir == DIR_IN ? qmf::DIR_IN : qmf::DIR_OUT);
    return prop;
}

bool Arg::addToMessage(DBusMessageIter *iter, const qVariant &v) const
{
    return message_add_arg(iter, v, type);
}

bool
message_add_arg(DBusMessageIter *iter, qVariant value,
                const char *signature)
{
    int type = DBUS_TYPE_INVALID;
    qList::iterator it, end;
    qList list;
    char *subsignature = NULL;
    DBusMessageIter subiter;
    DBusBasicValue v;

    if (!signature || strlen(signature) == 0) {
        mh_crit("Empty signature!");
        return false;
    }
    int len = strlen(signature), start;
    switch (signature[0]) {
        case DBUS_TYPE_ARRAY:
            try {
                list = value.asList();
            } catch (const qpid::types::InvalidConversion &err) {
                mh_crit("Unable to convert value to list: %s", err.what());
                return false;
            }

            subsignature = strdup(signature + 1);
            mh_debug("Array<%s>: %s", subsignature, value.asString().c_str());

            if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
                                                  subsignature, &subiter)) {
                mh_crit("Unable to open message iter container");
                return false;
            }

            end = list.end();
            for (it = list.begin(); it != end; it++) {
                if (!message_add_arg(&subiter, *it, subsignature)) {
                    return false;
                }
            }
            if (!dbus_message_iter_close_container(iter, &subiter)) {
                mh_crit("Unable to close DBus message iterator container");
                // Try to continue
            }
            free(subsignature);
            break;
        case DBUS_TYPE_STRUCT:
        case '(':
        case DBUS_TYPE_DICT_ENTRY:
        case '{':
            try {
                list = value.asList();
            } catch (const qpid::types::InvalidConversion &err) {
                mh_crit("Unable to convert value %s to list: %s",
                        value.asString().c_str(), err.what());
                return false;
            }

            start = 1;
            if (signature[0] == '(') {
                mh_debug("Struct<%s>: %s", signature, value.asString().c_str());
                if (!dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT,
                                                      NULL, &subiter)) {
                    mh_crit("Unable to open message iter container");
                    return false;
                }
            } else {
                mh_debug("Dict<%s>: %s", signature, value.asString().c_str());
                if (!dbus_message_iter_open_container(iter,
                        DBUS_TYPE_DICT_ENTRY, NULL, &subiter)) {
                    mh_crit("Unable to open message iter container");
                    return false;
                }
            }
            end = list.end();
            for (it = list.begin(); it != end; it++) {
                len = get_signature_item_length(signature + start);
                subsignature = strndup(signature + start, len);
                start += len;

                if (len < 1) {
                    mh_crit("Signature (%s) is too short\n", signature);
                    return false;
                }
                if (!message_add_arg(&subiter, *it, subsignature)) {
                    return false;
                }
                free(subsignature);
            }
            if (!dbus_message_iter_close_container(iter, &subiter)) {
                mh_crit("Unable to close DBus message iterator container");
                // Try to continue
            }
            break;
        case DBUS_TYPE_VARIANT:
            v = qpid_variant_to_dbus(value, &type, signature[0]);
            char s[2];
            s[0] = type;
            s[1] = '\0';
            dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, s,
                                             &subiter);
            dbus_message_iter_append_basic(&subiter, type, &v);
            dbus_message_iter_close_container(iter, &subiter);
            break;
        case DBUS_TYPE_INVALID:
            mh_crit("Invalid type in signature, this shouldn't happen");
            return false;
            break;
        default:
            // Handle DBus basic types
            v = qpid_variant_to_dbus(value, &type, signature[0]);
            if (type == DBUS_TYPE_INVALID) {
                mh_crit("Unable to convert %s to basic DBus type %c",
                        value.asString().c_str(), signature[0]);
                return false;

            }
            if (!dbus_message_iter_append_basic(iter, type, &v)) {
                mh_crit("Unable to add value %s to DBus argument of type %c",
                        value.asString().c_str(), type);
                return false;
            }
    }
    // Free the string
    if (type == DBUS_TYPE_STRING) {
        free(v.str);
    }

    return true;
}
