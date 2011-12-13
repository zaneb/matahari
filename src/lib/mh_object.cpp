/* mh_object.cpp - Copyright (C) 2011 Red Hat, Inc.
 * Written by Zane Bitter <zbitter@redhat.com>
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

#include <map>
#include <matahari/object.h>
#include <matahari/logging.h>

struct ObjectManagerImpl
{
    ObjectManagerImpl(qmf::AgentSession& session): _session(session) { }

    MatahariObject *find(const qmf::DataAddr& addr);


    // This is necessary to avoid a bug (#QPID3344) in libqmf2 versions prior
    // to 0.14, in which DataAddr objects are not compared correctly when they
    // are const (as all std::map keys are).
    struct AddrComparator {
        bool operator()(const qmf::DataAddr& addr1, const qmf::DataAddr& addr2)
        {
            return const_cast<qmf::DataAddr&>(addr1) < addr2;
        }
    };

    typedef std::map<qmf::DataAddr, MatahariObject*, AddrComparator> ObjectMap;


    qmf::AgentSession& _session;
    ObjectMap _objects;
};


MatahariObject*
ObjectManagerImpl::find(const qmf::DataAddr& addr)
{
    ObjectMap::const_iterator it(_objects.find(addr));

    return it != _objects.end() ? it->second : NULL;
}


ObjectManager::ObjectManager(qmf::AgentSession& session):
    _impl(new ObjectManagerImpl(session))
{ }

ObjectManager::~ObjectManager()
{
    delete _impl;
}

ObjectManager& ObjectManager::operator+=(MatahariObject* object)
{
    object->publish(_impl->_session);
    if (!object->getAddr().isValid()) {
        mh_err("Failed to publish object");
        return *this;
    }
    _impl->_objects[object->getAddr()] = object;

    return *this;
}

ObjectManager& ObjectManager::operator-=(MatahariObject* object)
{
    _impl->_objects.erase(object->getAddr());
    object->withdraw(_impl->_session);

    return *this;
}

bool ObjectManager::invoke(qmf::AgentSession& session,
                           qmf::AgentEvent& event) const
{
    if (event.hasDataAddr()) {
        MatahariObject *obj = _impl->find(event.getDataAddr());

        if (obj) {
            return obj->invoke(session, event);
        }

        mh_err("Target of method call not found");
    }

    return false;
}


MatahariObject::MatahariObject(qmf::Schema schema): _instance(schema)
{ }

MatahariObject::~MatahariObject()
{ }

void
MatahariObject::publish(qmf::AgentSession& session)
{
    session.addData(_instance);
    mh_debug("Published object %s", getAddr().getName().c_str());
}

void
MatahariObject::withdraw(qmf::AgentSession& session)
{
    mh_debug("Withdrawing object %s", getAddr().getName().c_str());
    session.delData(getAddr());
}
