/* object.h - Copyright (C) 2011 Red Hat, Inc.
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

#ifndef __MATAHARI_OBJECT_H
#define __MATAHARI_OBJECT_H

#include <qmf/AgentEvent.h>
#include <qmf/AgentSession.h>
#include <qmf/Schema.h>
#include <qmf/Data.h>
#include <qmf/DataAddr.h>

/**
 * \file
 * \brief Utilities for managing multiple objects
 */


/**
 * An object that is managed by an ObjectManager. Create a subclass of this and
 * override the invoke() method to handle events destined for the object.
 */
class MatahariObject
{
public:
    /** Create the object with the supplied schema. */
    MatahariObject(qmf::Schema schema);
    virtual ~MatahariObject();

    /** Get the QMF address of the object. */
    const qmf::DataAddr& getAddr(void) { return _instance.getAddr(); }

    /** Publish the object in the specified session. */
    void publish(qmf::AgentSession& session);
    /** Unpublish the object from the specified session. */
    void withdraw(qmf::AgentSession& session);

    /**
     * Handle an event that is destined for this object.
     * \param session the session from which the event originated.
     * \param event the event
     * \return true if the event was handled, false otherwise.
     */
    virtual bool invoke(qmf::AgentSession& session, qmf::AgentEvent& event)
        { return false; }

protected:
    qmf::Data _instance;
};


class ObjectManagerImpl;

/**
 * A store that manages a collection of QMF objects.
 */
class ObjectManager
{
public:
    /** Initialise for the given session. */
    ObjectManager(qmf::AgentSession& session);
    ~ObjectManager();

    /** Publish a new object to the session. */
    ObjectManager& operator+=(MatahariObject* object);
    /** Unpublish an object from the session. */
    ObjectManager& operator-=(MatahariObject* object);

    /**
     * Handle an event by directing it to the correct object.
     * \param session the session from which the event originated.
     * \param event the event
     * \return true if the event was handled, false otherwise.
     */
    bool invoke(qmf::AgentSession& session, qmf::AgentEvent& event) const;

private:
    ObjectManagerImpl *_impl;
};

#endif
