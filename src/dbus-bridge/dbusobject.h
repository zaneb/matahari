/* dbusobject.h - Copyright (C) 2011 Red Hat, Inc.
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

#ifndef MH_DBUSBRIDGE_DBUSOBJECT_H
#define MH_DBUSBRIDGE_DBUSOBJECT_H

#include "utils.h"

struct DBusObject {
    bool listenForSignals;
    string bus_name;
    string object_path;
    list<Interface *> interfaces;
    DBusConnection *connection;
    qmf::AgentSession *session;

    /**
     * Creates new DBus object from bridging between qmf and dbus
     *
     * \param[in] DBusConnection connection to system bus
     * \param[in] bus_name bus name of DBus object
     * \param[in] object_path path of the DBus object
     * \param[in] listenForSignals True if DBus signals should be transformed
     *                             to QMF events
     * \param[out] error pointer to NULL if no error occurs, GError otherwise
     */
    DBusObject(DBusConnection *conn, const char *bus_name,
               const char *object_path, bool listenForSignals,
               GError **error);
    virtual ~DBusObject();

    /**
     * Convert DBus object to QMF schema and add it to \p session
     *
     * \param[in] session QMF session
     * \retval True addition succeeds
     * \retval False error occured
     */
    bool addToSchema(qmf::AgentSession &session);
    const Interface *getInterface(const string &name) const;
    bool signalReceived(DBusMessage *message);
};

#endif
