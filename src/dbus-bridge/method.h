/* method.h - Copyright (C) 2011 Red Hat, Inc.
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

#ifndef MH_DBUSBRIDGE_METHOD_H
#define MH_DBUSBRIDGE_METHOD_H

#include "utils.h"

struct Method {
    string name;
    list<Arg *> inArgs;
    list<Arg *> outArgs;
    list<Arg *> allArgs;
    const Interface *interface;

    Method(xmlNode *node, const Interface *iface);
    virtual ~Method();
    void addArg(Arg *arg);
    list<qpid::types::Variant> argMapToList(const map<string, qpid::types::Variant> &m) const;

    list<qpid::types::Variant> call(const list<qpid::types::Variant> &args, GError **err) const;
};

struct Signal {
    string name;
    list<Arg *> args;
    const Interface *interface;
    qmf::Schema *schema;

    Signal(xmlNode *node, const Interface *iface);
    virtual ~Signal();
};

enum { DIR_IN, DIR_OUT };
enum { ACCESS_READ_ONLY, ACCESS_READ_WRITE };

struct Arg {
    string name;
    char *type;
    int dir;
    int access;

    Arg(xmlNode *node, int index);
    virtual ~Arg();
    qmf::SchemaProperty toSchemaProperty() const;
    bool addToMessage(DBusMessageIter *iter, const qpid::types::Variant &v) const;
};

#endif
