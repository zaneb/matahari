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

/**
 * DBusObject is internal representation of one DBus object.
 */
class DBusObject {
private:
    bool listenForSignals;

    list<Method *> methods;
    list<Signal *> signals;
    list<Arg *> properties;

    qmf::AgentSession *session;

public:
    DBusConnection *connection;
    string bus_name;
    string object_path;
    string interface;
    /** isRegistered is true when object has been added to schema */
    bool isRegistered;

    /**
     * Creates new DBus object from bridging between qmf and dbus
     *
     * \param[in] DBusConnection connection to system bus
     * \param[in] bus_name bus name of DBus object
     * \param[in] object_path path of the DBus object
     * \param[in] interface interface that is managed by object
     * \param[in] listenForSignals True if DBus signals should be transformed
     *                             to QMF events
     * \param[out] error pointer to NULL if no error occurs, GError otherwise
     */
    DBusObject(DBusConnection *conn, const char *bus_name,
               const char *object_path, const char *interface,
               bool listenForSignals, GError **error);
    virtual ~DBusObject();

    /**
     * Convert DBus object to QMF schema and add it to \p session
     *
     * \param[in] session QMF session
     * \param[out] error pointer to NULL if no error occurs, GError otherwise
     */
    void addToSchema(qmf::AgentSession &session, GError **error);

    /**
     * Method which is invoked when signal is received from registered
     * dbus object. It converts arguments from DBus message to QMF event
     * properties and then raises QMF event.
     *
     * \param[in] message DBusMessage that contains data from the signal
     * \retval True Signal was successfully processed
     * \retval False Processing of signal failed
     */
    bool signalReceived(DBusMessage *message) const;

    /**
     * Get method by name
     *
     * \param[in] name name of the method
     * \return Method object or NULL when not found
     */
    const Method *getMethod(const string &name) const;

    /**
     * Get signal by name
     *
     * \param[in] name name of the signal
     * \return Signal object or NULL when not found
     */
    const Signal *getSignal(const string &name) const;

    /**
     * Get property by name
     *
     * \param[in] name name of the property
     * \return Arg object or NULL when not found
     */
    const Arg *getProperty(const string &name) const;


    // Static methods

    /**
     * Call DBus method without creating DBusObject instance
     *
     * \param[in] conn pointer to valid DBusConnection
     * \param[in] bus_name bus name of the object
     * \param[in] object_path path of the object
     * \param[in] interface name of the interface
     * \param[in] method name of the method
     * \param[in] args arguments for method call
     * \param[out] err GError if method call fails
     *                 (pointer to NULL if call succeeds)
     * \return list with output arguments from the method
     */
    static qList call(DBusConnection *conn, const char *bus_name,
                      const char *object_path, const char *interface,
                      const char *method, const qList &args, GError **err);

    /**
     * Get the list of all subordinate object paths for given bus name
     *
     * \param[in] conn pointer to valid DBusConnection
     * \param[in] bus_name bus name of the object
     * \param[in] object_path base object path
     * \return list of object paths that have some interfaces
     */
    static qList getObjectPaths(DBusConnection *conn, const string &bus_name,
                                const string &object_path);

    /**
     * Get the list of interfaces
     *
     * \param[in] conn pointer to valid DBusConnection
     * \param[in] bus_name bus name of the object
     * \param[in] object_path path of the object
     * \return list of interfaces for given dbus object
     */
    static qList getInterfaces(DBusConnection *conn, const string &bus_name,
                               const string &object_path);

};

#endif
