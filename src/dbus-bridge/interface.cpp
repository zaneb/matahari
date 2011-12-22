/* interface.cpp - Copyright (C) 2011 Red Hat, Inc.
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

#include "interface.h"

#include "method.h"
#include "signal.h"
#include <libxml/xpath.h>

Interface::Interface(xmlNode *node, const DBusObject *obj) : dbusObject(obj)
{
    xmlNode *subnode;
    name = string((const char *) xmlGetProp(node, (const xmlChar *) "name"));
    if (name.size() > 0) {
        for (subnode = xmlFirstElementChild(node);
             subnode; subnode = subnode->next) {

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
    }
}

Interface::~Interface()
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
}

const Method *Interface::getMethod(const string &_name) const
{
    list<Method *>::const_iterator it, end = methods.end();
    for (it = methods.begin(); it != end; it++) {
        if ((*it)->name.compare(_name) == 0) {
            return *it;
        }
    }
    return NULL;
}

const Signal *Interface::getSignal(const string &_name) const
{
    list<Signal *>::const_iterator it, end = signals.end();
    for (it = signals.begin(); it != end; it++) {
        if ((*it)->name.compare(_name) == 0) {
            return *it;
        }
    }
    return NULL;
}
