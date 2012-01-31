/* utils.h - Copyright (C) 2011 Red Hat, Inc.
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

#ifndef MH_DBUSBRIDGE_UTILS_H
#define MH_DBUSBRIDGE_UTILS_H

#include <glib/gerror.h>
#include <dbus/dbus.h>
#include <string>
#include <map>
#include <list>

using namespace std;

struct DBusMessageIter;

namespace qpid {
    namespace types {
        class Variant;
    }
}

#define qList std::list<qpid::types::Variant>
#define qMap std::map<string, qpid::types::Variant>
#define qVariant qpid::types::Variant

namespace qmf {
    class AgentSession;
    class Schema;
    class SchemaProperty;
}

struct DBusObject;
struct Interface;
struct Method;
struct Signal;
struct Arg;

typedef struct _xmlNode xmlNode;

typedef union
{
    bool          boolean;
    dbus_int16_t  i16;
    dbus_uint16_t u16;
    dbus_int32_t  i32;
    dbus_uint32_t u32;
    dbus_int64_t  i64;
    dbus_uint64_t u64;
    double        dbl;
    unsigned char byt;
    char         *str;
} DBusBasicValue;

#define MATAHARI_ERROR matahari_error_quark()
GQuark
matahari_error_quark(void);

// Default timeout for all calls with timeout
#define CALL_TIMEOUT 10000

#define DBUS_INTROSPECTABLE "org.freedesktop.DBus.Introspectable"
#define DBUS_INTROSPECT "Introspect"
#define PROPERTIES_IFACE "org.freedesktop.DBus.Properties"

/**
 * Convert DBus type to QMF type
 *
 * \param[in] type DBus type
 *
 * \return QMF type
 */
int
dbus_type_to_qmf_type(int type);

/**
 * Get lenght of one item from DBus signature, one item means basic type,
 * array, struct or dict entry.
 *
 * \param[in] signature DBus signature
 *
 * \return number of characters of the first item
 */
int
get_signature_item_length(const char *signature);


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
qpid_variant_to_dbus(const qVariant &value, int *type, int expected_type);


/**
 * Convert DBus message iterator of type \p type to qpid Variant.
 *
 * \param[in] type Type of DBus message
 * \param[in] iter DBus message iterator
 *
 * \return qpid Variant
 */
qpid::types::Variant
dbus_message_iter_to_qpid_variant(int type, DBusMessageIter *iter);

/**
 * Use DBus introspection to obtain XML with description of the object
 *
 * \param[in] conn Valid connection to DBus bus
 * \param[in] bus_name Name of the object on bus
 * \param[in] object_path Path of the object
 * \param[out] error Pointer to null if succeeds, pointer to error otherwise
 *
 * \return DBus introspection XML, do not free
 */
const char *
dbus_introspect(DBusConnection *conn, string bus_name, string object_path, GError **error);

#endif
