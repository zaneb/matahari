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

#include "matahari/agent.h"
#include "qmf/org/matahariproject/QmfPackage.h"
#include <iostream>
#include <sstream>
#include <assert.h>

extern "C" {
    #include "matahari/utilities.h"
    #include "matahari/logging.h"
    #include <stdio.h>
    #include <dbus/dbus.h>
    #include <libxml/xpath.h>
}

#define DBUS_INTROSPECTABLE "org.freedesktop.DBus.Introspectable"
#define DBUS_INTROSPECT "Introspect"

class DBusAgent : public MatahariAgent
{
public:
    virtual int setup(qmf::AgentSession session);
    virtual gboolean invoke(qmf::AgentSession session, qmf::AgentEvent event,
                            gpointer user_data);

private:
    qmf::org::matahariproject::PackageDefinition _package;
    qmf::Data _instance;
    static const char DBUS_BRIDGE_NAME[];
    DBusConnection *conn;
};

const char DBusAgent::DBUS_BRIDGE_NAME[] = "DBusBridge";

int
DBusAgent::setup(qmf::AgentSession session)
{
    DBusError *err = NULL;
    _package.configure(session);
    _instance = qmf::Data(_package.data_DBusBridge);

    //_instance.setProperty("uuid", mh_uuid());
    _instance.setProperty("hostname", mh_hostname());

    session.addData(_instance, DBUS_BRIDGE_NAME);

    conn = dbus_bus_get(DBUS_BUS_SYSTEM, err);
    // TODO: error handling

    return 0;
}


typedef union
{
  dbus_int16_t  i16;
  dbus_uint16_t u16;
  dbus_int32_t  i32;
  dbus_uint32_t u32;
  dbus_int64_t  i64;
  dbus_uint64_t u64;
  double dbl;
  unsigned char byt;
  char *str;
} DBusBasicValue;

/**
 * Convert qpid Variant to DBus basic type.
 *
 * \param[in] value qpid Variant
 * \param[out] type pointer to type of DBusBasicValue result,
 *             DBUS_TYPE_INVALID if conversion fails
 * \param[in] expected_type type that the value should be converted to
 *                      (from DBus signature)
 *
 * \return converted result
 */
DBusBasicValue
qpid_variant_to_dbus(const qpid::types::Variant &value, int *type, int expected_type)
{
    mh_debug("qpid_variant_to_dbus: %s (exp: %c)\n", value.asString().c_str(), expected_type);

    DBusBasicValue v;
    *type = DBUS_TYPE_INVALID;
    try {
        switch (value.getType()) {
            case qpid::types::VAR_VOID:
                break;
            case qpid::types::VAR_BOOL:
                if (expected_type != DBUS_TYPE_BOOLEAN &&
                        expected_type != DBUS_TYPE_VARIANT) {
                    mh_crit("Unable to convert non-boolean to boolean\n");
                    return v;
                }
                v.i32 = value.asBool();
                *type = DBUS_TYPE_BOOLEAN;
                break;
            case qpid::types::VAR_INT8:
            case qpid::types::VAR_INT16:
            case qpid::types::VAR_INT32:
            case qpid::types::VAR_INT64:
            case qpid::types::VAR_UINT8:
            case qpid::types::VAR_UINT16:
            case qpid::types::VAR_UINT32:
            case qpid::types::VAR_UINT64:
                switch (expected_type) {
                    case DBUS_TYPE_BYTE:
                        v.byt = value.asUint8();
                        break;
                    case DBUS_TYPE_INT16:
                        v.i16 = value.asInt16();
                        break;
                    case DBUS_TYPE_INT32:
                        v.i32 = value.asInt32();
                        break;
                    case DBUS_TYPE_INT64:
                        v.i64 = value.asInt64();
                        break;
                    case DBUS_TYPE_UINT16:
                        v.u16 = value.asUint16();
                        break;
                    case DBUS_TYPE_UINT32:
                        v.u32 = value.asUint32();
                        break;
                    case DBUS_TYPE_UINT64:
                        v.u64 = value.asUint64();
                        break;
                    case DBUS_TYPE_DOUBLE:
                        v.dbl = (double) value.asInt64();
                        break;
                    case DBUS_TYPE_VARIANT:
                        // TODO: maybe uint64
                        v.i64 = value.asInt64();
                        *type = DBUS_TYPE_INT64;
                        return v;
                        break;
                    default:
                        mh_crit("Unable to convert number to %c\n", expected_type);
                        return v;
                }
                *type = expected_type;
                break;
            case qpid::types::VAR_FLOAT:
            case qpid::types::VAR_DOUBLE:
                if (expected_type != DBUS_TYPE_DOUBLE &&
                        expected_type != DBUS_TYPE_VARIANT) {
                    mh_crit("Unable to convert to double\n");
                    return v;
                }
                v.dbl = value.asDouble();
                *type = DBUS_TYPE_DOUBLE;
                break;
            case qpid::types::VAR_STRING:
                if (expected_type != DBUS_TYPE_STRING &&
                        expected_type != DBUS_TYPE_VARIANT) {
                    mh_crit("Unable to convert to string\n");
                    return v;
                }
                v.str = strdup(value.asString().c_str());
                *type = DBUS_TYPE_STRING;
                break;
            default:
                mh_crit("Unable to use qpid_variant_to_dbus on non-simple variant %s\n", qpid::types::getTypeName(value.getType()).c_str());
                break;
        }
    } catch (const qpid::types::InvalidConversion &err) {
        mh_crit("Invalid conversion: %s\n", err.what());
        *type = DBUS_TYPE_INVALID;
        return v;
    }
    return v;
}

/**
 * Get lenght of one item from DBus signature, one item means basic type,
 * array, struct or dict entry.
 *
 * \param[in] signature DBus signature
 *
 * \return number of characters for first item
 */
int
get_signature_item_length(const char *signature)
{
    if (!signature || strlen(signature) == 0)
        return 0;
    int ends = 1, pos = 0;
    switch (signature[0]) {
        case DBUS_TYPE_ARRAY:
            // Signature for array is: 'a' + one following item
            return 1 + get_signature_item_length(signature + 1);
            break;
        case DBUS_TYPE_STRUCT:
        case '(':
            // Signature for struct is everything between brackets
            // But struct may contain another struct, so we must count
            // starting and ending brackets.
            while (ends > 0 && signature[pos + 1] != '\0') {
                pos += 1;
                if (signature[pos] == '(') {
                    ends++;
                } else if (signature[pos] == ')') {
                    ends--;
                }
            }
            return pos + 1;
            break;
        case DBUS_TYPE_DICT_ENTRY:
        case '{':
            // Same as struct, find matching curly bracket
            while (ends > 0 && signature[pos + 1] != '\0') {
                pos += 1;
                if (signature[pos] == '{') {
                    ends++;
                } else if (signature[pos] == '}') {
                    ends--;
                }
            }
            return pos + 1;
            break;
        default:
            return 1;
    }
    return 0;
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
gboolean
message_add_arg(DBusMessageIter *iter, qpid::types::Variant value,
                const char *signature)
{
    int type = DBUS_TYPE_INVALID;
    qpid::types::Variant::List::iterator it, end;
    qpid::types::Variant::List list;
    char *subsignature = NULL;
    DBusMessageIter subiter;
    DBusBasicValue v;

    if (!signature || strlen(signature) == 0) {
        mh_crit("Empty signature!\n");
        return FALSE;
    }
    int len = strlen(signature), start;
    mh_debug("message_add_arg with signature: %s and value %s\n", signature, value.asString().c_str());
    switch (signature[0]) {
        case DBUS_TYPE_ARRAY:
            try {
                list = value.asList();
            } catch (const qpid::types::InvalidConversion &err) {
                mh_crit("Unable to convert value to list: %s", err.what());
                return FALSE;
            }

            subsignature = strdup(signature + 1);
            mh_debug("Array<%s>: %s\n", subsignature, value.asString().c_str());

            assert(dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, subsignature, &subiter));

            end = list.end();
            for (it = list.begin(); it != end; it++) {
                if (!message_add_arg(&subiter, *it, subsignature)) {
                    return FALSE;
                }
            }
            assert(dbus_message_iter_close_container(iter, &subiter));
            free(subsignature);
            break;
        case DBUS_TYPE_STRUCT:
        case '(':
        case DBUS_TYPE_DICT_ENTRY:
        case '{':

            try {
                list = value.asList();
            } catch (const qpid::types::InvalidConversion &err) {
                mh_crit("Unable to convert value to list: %s", err.what());
                return FALSE;
            }

            start = 1;
            if (signature[0] == '(') {
                assert(dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &subiter));
            } else {
                assert(dbus_message_iter_open_container(iter, DBUS_TYPE_DICT_ENTRY, NULL, &subiter));
            }
            end = list.end();
            for (it = list.begin(); it != end; it++) {
                len = get_signature_item_length(signature + start);
                subsignature = strndup(signature + start, len);
                start += len;
                mh_debug("Subsignature: %s\n", subsignature);

                if (len < 1) {
                    mh_crit("Signature (%s) is too short\n", signature);
                    return FALSE;
                }
                if (!message_add_arg(&subiter, *it, subsignature)) {
                    return FALSE;
                }
                free(subsignature);
            }
            assert(dbus_message_iter_close_container(iter, &subiter));
            break;
        case DBUS_TYPE_VARIANT:
            v = qpid_variant_to_dbus(value, &type, signature[0]);
            char s[2];
            s[0] = type;
            s[1] = '\0';
            dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, s, &subiter);
            dbus_message_iter_append_basic(&subiter, type, &v);
            dbus_message_iter_close_container(iter, &subiter);
            break;
        case DBUS_TYPE_INVALID:
            mh_crit("Invalid type in signature, this shouldn't happen\n");
            return FALSE;
            break;
        default:
            v = qpid_variant_to_dbus(value, &type, signature[0]);
            if (type == DBUS_TYPE_INVALID) {
                mh_crit("Unable to convert %s to basic DBus type %c\n", value.asString().c_str(), signature[0]);
                return FALSE;

            }
            mh_debug("Basic type %c\n", type);
            if (!dbus_message_iter_append_basic(iter, type, &v)) {
                mh_crit("Unable to add value %s to DBus argument of type %c\n", value.asString().c_str(), type);
                return FALSE;
            }
    }
    // Free the string
    if (type == DBUS_TYPE_STRING) {
        free(v.str);
    }

    return TRUE;
}

/**
 * Convert DBus message iterator of type \p type to qpid Variant.
 *
 * \param[in] type Type of DBus message
 * \param[in] iter DBus message iterator
 *
 * \return qpid Variant
 */
qpid::types::Variant
dbus_message_iter_to_qpid_variant(int type, DBusMessageIter *iter)
{
    DBusBasicValue v;
    qpid::types::Variant::List l;
    DBusMessageIter subiter;
    int subtype;


    switch (type) {
        case DBUS_TYPE_INVALID:
            return qpid::types::Variant();
            break;
        case DBUS_TYPE_BOOLEAN:
            dbus_message_iter_get_basic(iter, &v.i32);
            return qpid::types::Variant(v.i32);
            break;
        case DBUS_TYPE_BYTE:
            dbus_message_iter_get_basic(iter, &v.byt);
            return qpid::types::Variant(v.byt);
            break;
        case DBUS_TYPE_INT16:
            dbus_message_iter_get_basic(iter, &v.i16);
            return qpid::types::Variant(v.i16);
            break;
        case DBUS_TYPE_INT32:
            dbus_message_iter_get_basic(iter, &v.i32);
            return qpid::types::Variant(v.i32);
            break;
        case DBUS_TYPE_INT64:
            dbus_message_iter_get_basic(iter, &v.i64);
            return qpid::types::Variant(v.i64);
            break;
        case DBUS_TYPE_UINT16:
            dbus_message_iter_get_basic(iter, &v.u16);
            return qpid::types::Variant(v.u16);
            break;
        case DBUS_TYPE_UINT32:
            dbus_message_iter_get_basic(iter, &v.u32);
            return qpid::types::Variant(v.u32);
            break;
        case DBUS_TYPE_UINT64:
            dbus_message_iter_get_basic(iter, &v.u64);
            return qpid::types::Variant(v.u64);
            break;
        case DBUS_TYPE_DOUBLE:
            dbus_message_iter_get_basic(iter, &v.dbl);
            return qpid::types::Variant(v.dbl);
            break;
        case DBUS_TYPE_OBJECT_PATH:
        case DBUS_TYPE_STRING:
            dbus_message_iter_get_basic(iter, &v.str);
            return qpid::types::Variant(v.str);
            break;
        case DBUS_TYPE_ARRAY:
        case DBUS_TYPE_STRUCT:
        case DBUS_TYPE_DICT_ENTRY:
            dbus_message_iter_recurse(iter, &subiter);
            while ((subtype = dbus_message_iter_get_arg_type (&subiter)) != DBUS_TYPE_INVALID) {
                l.push_back(dbus_message_iter_to_qpid_variant(subtype, &subiter));
                dbus_message_iter_next(&subiter);
            }
            return l;
            break;
        case DBUS_TYPE_VARIANT:
            dbus_message_iter_recurse(iter, &subiter);
            return dbus_message_iter_to_qpid_variant(dbus_message_iter_get_arg_type(&subiter), &subiter);
            break;
        default:
            mh_debug("Unknown output argument: %c\n", type);
            break;
    }
    return qpid::types::Variant();
}

/**
 * Print DBus message arguments
 *
 * \param[in] iter DBus message iterator
 */
static void
print_iter (DBusMessageIter *iter)
{
    do {
        int type = dbus_message_iter_get_arg_type (iter);
        const char *str;
        dbus_uint32_t uint32;
        dbus_int32_t int32;
        dbus_uint64_t uint64;
        dbus_int64_t int64;
        double d;
        unsigned char byte;
        dbus_bool_t boolean;
        DBusMessageIter subiter;

        if (type == DBUS_TYPE_INVALID)
            break;

        switch (type) {
            case DBUS_TYPE_OBJECT_PATH :
            case DBUS_TYPE_STRING:
                dbus_message_iter_get_basic (iter, &str);
                mh_debug("string '%s'\n", str);
                break;
            case DBUS_TYPE_INT32:
                dbus_message_iter_get_basic (iter, &int32);
                mh_debug("int32 %d\n", int32);
                break;
            case DBUS_TYPE_INT64:
                dbus_message_iter_get_basic (iter, &int64);
                mh_debug("int64 %ld\n", int64);
                break;
            case DBUS_TYPE_UINT32:
                dbus_message_iter_get_basic (iter, &uint32);
                mh_debug("uint32 %u\n", uint32);
                break;
            case DBUS_TYPE_UINT64:
                dbus_message_iter_get_basic (iter, &uint64);
                mh_debug("int64 %ld\n", uint64);
                break;
            case DBUS_TYPE_DOUBLE:
                dbus_message_iter_get_basic (iter, &d);
                mh_debug("double %g\n", d);
                break;
            case DBUS_TYPE_BYTE:
                dbus_message_iter_get_basic (iter, &byte);
                mh_debug("byte %d\n", byte);
                break;
            case DBUS_TYPE_BOOLEAN:
                dbus_message_iter_get_basic (iter, &boolean);
                mh_debug("boolean %s\n", boolean ? "true" : "false");
                break;
            case DBUS_TYPE_VARIANT:
                dbus_message_iter_recurse (iter, &subiter);
                mh_debug("variant:");
                print_iter (&subiter);
                break;
            case DBUS_TYPE_ARRAY:
                int current_type;

                dbus_message_iter_recurse (iter, &subiter);

                mh_debug("[");
                while ((current_type = dbus_message_iter_get_arg_type (&subiter))
                    != DBUS_TYPE_INVALID)
                {
                    print_iter (&subiter);
                    dbus_message_iter_next (&subiter);
                    if (dbus_message_iter_get_arg_type (&subiter) != DBUS_TYPE_INVALID)
                        mh_debug(",");
                }
                mh_debug("]");
                break;
            case DBUS_TYPE_DICT_ENTRY:
                dbus_message_iter_recurse (iter, &subiter);
                mh_debug("{");
                print_iter (&subiter);
                dbus_message_iter_next (&subiter);
                print_iter (&subiter);
                mh_debug("}");
                break;
            case DBUS_TYPE_STRUCT:
                dbus_message_iter_recurse (iter, &subiter);
                mh_debug("(");
                while ((current_type = dbus_message_iter_get_arg_type (&subiter))
                    != DBUS_TYPE_INVALID)
                {
                    print_iter (&subiter);
                    dbus_message_iter_next (&subiter);
                    if (dbus_message_iter_get_arg_type (&subiter) != DBUS_TYPE_INVALID)
                        mh_debug(",");
                }
                mh_debug(")");
                break;
            default:
                mh_debug("error: unable to decipher arg type '%c'\n", type);
                break;
        }
    } while (dbus_message_iter_next (iter));
}

/**
 * Print DBus message
 *
 * \param[in] message Message to be printed
 */
void
print_message(DBusMessage *message)
{
    DBusMessageIter iter;
    dbus_message_iter_init (message, &iter);
    print_iter(&iter);
}

/**
 * Get signature of DBus method using DBus introspection.
 * TODO: What to do if DBus introspection doesn't not work? Probably use qmf
 *       arguments to guess the signature
 *
 * \param[in] conn DBus connection
 * \param[in] bus_name DBus object name
 * \param[in] object_path path of DBus object
 * \param[in] interface DBus interface
 * \param[in] method_name name of the method
 *
 * \param[out] in_signature List of signatures for input arguments
 * \param[out] out_signature List of signatures for output arguments
 * TODO: out signature not implemented yet
 */
void
get_dbus_method_signature(DBusConnection *conn,
                       const char *bus_name, const char *object_path,
                       const char *interface, const char *method_name,
                       vector<char *> &in_signature, const vector<char *> &out_signature)
{
    DBusError err;
    DBusMessage *message, *reply;
    const char *xml;
    char *xpath;

    // Call Introspect method
    dbus_error_init(&err);
    message = dbus_message_new_method_call(bus_name, object_path, DBUS_INTROSPECTABLE, DBUS_INTROSPECT);
    reply = dbus_connection_send_with_reply_and_block(conn, message, 10000, &err);
    dbus_message_get_args(reply, &err, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);

    // Use xpath to obtain signatures for arguments
    asprintf(&xpath, "/node/interface[@name=\"%s\"]/method[@name=\"%s\"]/arg[@direction=\"in\"]/@type", interface, method_name);
    xmlChar *xxpath = xmlCharStrdup(xpath);
    free(xpath);
    xmlInitParser();
    xmlDocPtr doc = xmlReadMemory(xml, strlen(xml), "noname.xml", NULL, 0);
    if (doc == NULL) {
        mh_crit("Unable to parse introspected XML\n");
        return;
    }
    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xxpath, xpathCtx);

    for (int i = 0; i < xpathObj->nodesetval->nodeNr; i++) {
        in_signature.push_back((char *) xmlStrdup(xpathObj->nodesetval->nodeTab[i]->children->content));
    }
    dbus_message_unref(message);
    dbus_message_unref(reply);
    free(xxpath);
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    xmlCleanupParser();
}

gboolean
DBusAgent::invoke(qmf::AgentSession session, qmf::AgentEvent event,
                            gpointer user_data)
{
    if (event.getType() != qmf::AGENT_METHOD) {
        return TRUE;
    }

//     enum mh_result res = MH_RES_SUCCESS;
    const std::string& methodName(event.getMethodName());
    qpid::types::Variant::Map& args = event.getArguments();

    if (methodName == "call") {
        mh_debug("===== Method call =====\n");
        mh_debug("Bus name: %s\nObject path: %s\nInterface: %s\nMethod name: %s\n",
                args["bus_name"].asString().c_str(),
                args["object_path"].asString().c_str(),
                args["interface"].asString().c_str(),
                args["method_name"].asString().c_str());

        qpid::types::Variant::List &method_args = args["args"].asList();
        DBusError err;
        DBusMessage *message, *reply;

        // Create method call
        message = dbus_message_new_method_call(
                args["bus_name"].asString().c_str(),
                args["object_path"].asString().c_str(),
                args["interface"].asString().c_str(),
                args["method_name"].asString().c_str());

        // Get signature of the method
        vector<char *> in_signature, out_signature;
        get_dbus_method_signature(conn, args["bus_name"].asString().c_str(),
                                  args["object_path"].asString().c_str(),
                                  args["interface"].asString().c_str(),
                                  args["method_name"].asString().c_str(),
                                  in_signature, out_signature);
        mh_debug("In signature: ");
        for (unsigned int i = 0; i < in_signature.size(); i++)
            mh_debug("%s, ", in_signature[i]);
        mh_debug("\n");

        // Convert qpid arguments to DBus arguments
        qpid::types::Variant::List::iterator it, end = method_args.end();
        DBusMessageIter iter;
        dbus_message_iter_init_append(message, &iter);
        int i = 0;
        if (in_signature.size() != method_args.size()) {
            mh_crit("Wrong number of arguments (expected: %lu, found: %lu)\n",
                    in_signature.size(), method_args.size());

        }
        for (it = method_args.begin(); it != end; it++) {
            if (!message_add_arg(&iter, *it, in_signature[i])) {
                std::stringstream str;
                str << "Argument " << i << " error";
                session.raiseException(event, str.str());
                return TRUE;
            }
            free(in_signature[i]);
            i++;
        }
        in_signature.clear();

        mh_debug("=== Message ===\n");
        print_message(message);
        mh_debug("\n===============\n");

        // TODO: Async method call
        dbus_error_init (&err);
        // Call method
        reply = dbus_connection_send_with_reply_and_block(conn, message, 10000, &err);

        if (!reply) {
            session.raiseException(event, err.message);
            mh_crit("Error during DBus call: %s\n", err.message);
            return TRUE;
        }

        mh_debug("=== Reply ===\n");
        print_message(reply);
        mh_debug("\n==============\n");

        // Convert return arguments from reply message
        dbus_message_iter_init(reply, &iter);
        int current_type;
        qpid::types::Variant::List list;
        while ((current_type = dbus_message_iter_get_arg_type(&iter)) != DBUS_TYPE_INVALID) {
            list.push_back(dbus_message_iter_to_qpid_variant(current_type, &iter));
            dbus_message_iter_next(&iter);
        }
        event.addReturnArgument("results", list);

        dbus_message_unref(message);
        dbus_message_unref(reply);
    } else {
        session.raiseException(event, mh_result_to_str(MH_RES_NOT_IMPLEMENTED));
        goto bail;
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
        agent.run();
    }

    return rc;
}

