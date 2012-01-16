/* interface.h - Copyright (C) 2011 Red Hat, Inc.
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

#ifndef MH_DBUSBRIDGE_INTERFACE_H
#define MH_DBUSBRIDGE_INTERFACE_H

#include "utils.h"

/**
 * Internal representation of DBus interface.
 */
struct Interface {
    string name;
    list<Method *> methods;
    list<Signal *> signals;
    list<Arg *> properties;
    const DBusObject *dbusObject;

    /**
     * Create Interace object from DBus introspection XML
     *
     * \param[in] node xmlNode with definition of interface
     * \param[in] obj DBusObject that contains the interface
     */
    Interface(xmlNode *node, const DBusObject *obj);
    virtual ~Interface();

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
};

#endif
