/* utils.cpp - Copyright (C) 2011 Red Hat, Inc.
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

#include "utils.h"

extern "C" {
    #include <dbus/dbus.h>
    #include "matahari/logging.h"
}

#include <qmf/SchemaTypes.h>
#include <qpid/types/Variant.h>

// Errors
GQuark
matahari_error_quark(void)
{
    return g_quark_from_static_string("matahari-error-quark");
}

int
dbus_type_to_qmf_type(int type)
{
    switch (type) {
        case DBUS_TYPE_BOOLEAN:
            return qmf::SCHEMA_DATA_BOOL;
        case DBUS_TYPE_BYTE:
        case DBUS_TYPE_INT16:
        case DBUS_TYPE_INT32:
        case DBUS_TYPE_INT64:
        case DBUS_TYPE_UINT16:
        case DBUS_TYPE_UINT32:
        case DBUS_TYPE_UINT64:
            return qmf::SCHEMA_DATA_INT;
        case DBUS_TYPE_DOUBLE:
            return qmf::SCHEMA_DATA_FLOAT;
        case DBUS_TYPE_OBJECT_PATH:
        case DBUS_TYPE_STRING:
            return qmf::SCHEMA_DATA_STRING;
        case DBUS_TYPE_VARIANT:
            // TODO: what return here?
            return qmf::SCHEMA_DATA_STRING;
        case DBUS_TYPE_ARRAY:
            return qmf::SCHEMA_DATA_LIST;
        case DBUS_TYPE_DICT_ENTRY:
        case '{':
            return qmf::SCHEMA_DATA_MAP;
        case DBUS_TYPE_STRUCT:
        case '(':
            return qmf::SCHEMA_DATA_LIST;
        default:
            mh_crit("Unknown DBus type %c", type);
    }
    return qpid::types::VAR_VOID;
}

bool
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
    switch (signature[0]) {
        case DBUS_TYPE_ARRAY:
            try {
                list = value.asList();
            } catch (const qpid::types::InvalidConversion &err) {
                mh_crit("Unable to convert value to list: %s", err.what());
                return FALSE;
            }

            subsignature = strdup(signature + 1);
            mh_debug("Array<%s>: %s", subsignature, value.asString().c_str());

            if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, subsignature, &subiter)) {
                mh_crit("Unable to open message iter container");
                return FALSE;
            }

            end = list.end();
            for (it = list.begin(); it != end; it++) {
                if (!message_add_arg(&subiter, *it, subsignature)) {
                    return FALSE;
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
                mh_crit("Unable to convert value %s to list: %s", value.asString().c_str(), err.what());
                return FALSE;
            }

            start = 1;
            if (signature[0] == '(') {
                mh_debug("Struct<%s>: %s", signature, value.asString().c_str());
                if (!dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &subiter)) {
                    mh_crit("Unable to open message iter container");
                    return FALSE;
                }
            } else {
                mh_debug("Dict<%s>: %s", signature, value.asString().c_str());
                if (!dbus_message_iter_open_container(iter, DBUS_TYPE_DICT_ENTRY, NULL, &subiter)) {
                    mh_crit("Unable to open message iter container");
                    return FALSE;
                }
            }
            end = list.end();
            for (it = list.begin(); it != end; it++) {
                len = get_signature_item_length(signature + start);
                subsignature = strndup(signature + start, len);
                start += len;

                if (len < 1) {
                    mh_crit("Signature (%s) is too short\n", signature);
                    return FALSE;
                }
                if (!message_add_arg(&subiter, *it, subsignature)) {
                    return FALSE;
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
            dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, s, &subiter);
            dbus_message_iter_append_basic(&subiter, type, &v);
            dbus_message_iter_close_container(iter, &subiter);
            break;
        case DBUS_TYPE_INVALID:
            mh_crit("Invalid type in signature, this shouldn't happen\n");
            return FALSE;
            break;
        default:
            // Handle DBus basic types
            v = qpid_variant_to_dbus(value, &type, signature[0]);
            if (type == DBUS_TYPE_INVALID) {
                mh_crit("Unable to convert %s to basic DBus type %c\n",
                        value.asString().c_str(), signature[0]);
                return FALSE;

            }
            if (!dbus_message_iter_append_basic(iter, type, &v)) {
                mh_crit("Unable to add value %s to DBus argument of type %c\n",
                        value.asString().c_str(), type);
                return FALSE;
            }
    }
    // Free the string
    if (type == DBUS_TYPE_STRING) {
        free(v.str);
    }

    return TRUE;
}

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

DBusBasicValue
qpid_variant_to_dbus(const qpid::types::Variant &value, int *type, int expected_type)
{
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

qpid::types::Variant
dbus_message_iter_to_qpid_variant(int type, DBusMessageIter *iter)
{
    DBusBasicValue v;
    qpid::types::Variant::List l;
    DBusMessageIter subiter;
    int subtype;

    switch (type) {
        case DBUS_TYPE_INVALID:
            mh_crit("Trying to convert DBUS_TYPE_INVALID to Variant\n");
            return qpid::types::Variant();
            break;
        case DBUS_TYPE_BOOLEAN:
            dbus_message_iter_get_basic(iter, &v.boolean);
            return qpid::types::Variant(v.boolean);
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
            return qpid::types::Variant(string(v.str));
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
            mh_crit("Unknown output argument: %c\n", type);
            break;
    }
    return qpid::types::Variant();
}
