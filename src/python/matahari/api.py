# Copyright (C) 2011 Red Hat, Inc.
# Written by Zane Bitter <zbitter@redhat.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import core

class BrokerConnection(object):
    def __init__(self, host='localhost', port=49000, ssl=False):
        self.host = host
        self.port = port
        self.ssl = ssl

class Proxy(object):
    def __init__(self, state):
        self._state = state
        self._objects = state.manager.get(state._class,
                                          state._package,
                                          state.agents(),
                                          **state._properties)
        self._schemata = set(o.getSchema() for o in self)
        self._methodnames = set(m.name for m in self._methods())

    def __getattr__(self, attr):
        if not self:
            raise IndexError("No objects selected")

        if attr in self._methodnames:
            def call_method(*args):
                return self._state.manager.invoke_method(self._objects,
                                                         attr,
                                                         *args)
            return call_method
        else:
            return [getattr(o, attr) for o in self]

    def __setattr__(self, attr, val):
        if attr.startswith('_'):
            self.__dict__[attr] = val
            return

        if not self:
            raise IndexError("No objects selected")

        for o in self:
            setattr(o, attr, val)

    def __str__(self):
        return str(self._objects)

    def __repr__(self):
        return repr(self._objects)

    def __len__(self):
        return len(self._objects)

    def __iter__(self):
        return iter(self._objects)

    def _set_from_schemata(self, transform):
        if self:
            return set.union(*[set(transform(s)) for s in self._schemata])
        else:
            return set()

    def _properties(self):
        return self._set_from_schemata(lambda s: s.getProperties())

    def _statistics(self):
        return self._set_from_schemata(lambda s: s.getStatistics())

    def _methods(self):
        return self._set_from_schemata(lambda s: s.getMethods())

class CacheDescriptor(object):
    def __init__(self, constructor):
        self.cache = {}
        self.constructor = constructor

    def __get__(self, instance, owner):
        if instance is None:
            return None
        if instance not in self.cache:
            if instance._class is None:
                proxy = None
            else:
                proxy = self.constructor(instance)
            self.cache[instance] = proxy
        return self.cache[instance]

    def __delete__(self, instance):
        if instance in self.cache:
            del self.cache[instance]

class Matahari(object):

    objects = CacheDescriptor(Proxy)

    def __init__(self, broker=BrokerConnection()):
        self._qmf = core.AsyncHandler()
        self.manager = core.Manager(broker.host, broker.port, broker.ssl,
                                    self._qmf)
        self._hosts = None
        self._package = None
        self._class = None
        self._properties = {}

    def set_hosts(self, hosts=None):
        """Filter on a list of hosts"""
        self._hosts = hosts
        del self.objects

    def set_class(self, klass, package='org.matahariproject'):
        """Filter on a particular class and package"""
        self._class = klass
        self._package = package
        self._properties = {}
        del self.objects

    def set_property_filter(self, name, value):
        """Add a filter on a property value"""
        self._properties[name] = value
        del self.objects

    def clear_property_filter(self):
        """Clear filters on property values"""
        self._properties = {}
        del self.objects

    def clear_state(self):
        """Clear all state"""
        self.clear_property_filter()
        self.set_class(None, None)
        self.set_hosts()

    def selected_classname(self):
        """Return the name of the currently selected class"""
        if self._class is None:
            return ''
        return '%s:%s' % (self._package, self._class)

    def list_hosts(self):
        """List all hosts with connected agents"""
        return set(self.manager.hosts())

    def list_packages(self):
        """List all available packages"""
        return set(self._qmf.packages)

    def list_classnames(self, package=None):
        """List all available classnames, optionally for only a given package"""
        classes = list(self._qmf.classes)
        pkgmatch = (package is None and
                    (lambda k: True) or
                    (lambda k: k.getPackageName() == package))
        return set([k.getClassName() for k in classes if pkgmatch(k)])

    def agents(self):
        """Return a list of all connected agents on the selected hosts"""
        return set(self.manager.agents(self._hosts))
