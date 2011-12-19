import core

class BrokerConnection(object):
    def __init__(self, host='localhost', port=49000, ssl=False):
        self.host = host
        self.port = 49000
        self.ssl = ssl

class Proxy(object):
    def __init__(self, state):
        self._state = state
        agents = state.manager.agents(state._hosts)
        self._objects = state.manager.get(state._class,
                                          package=state._package,
                                          agents=agents)
        self._schemata = set(o.getSchema() for o in self._objects)
        self._methodnames = set(m.name for m in self._methods())

    def __getattr__(self, attr):
        if attr in self._methodnames:
            def call_method(*args):
                return self._state.manager.invoke_method(self._objects,
                                                         attr,
                                                         *args)
            return call_method
        else:
            return [getattr(o, attr) for o in self._objects]

    def __setattr__(self, attr, val):
        if attr.startswith('_'):
            self.__dict__[attr] = val
            return

        for o in self._objects:
            setattr(o, attr, val)

    def __str__(self):
        return str(self._objects)

    def __repr__(self):
        return repr(self._objects)

    def _properties(self):
        return set.union(*[set(s.getProperties()) for s in self._schemata])

    def _statistics(self):
        return set.union(*[set(s.getStatistics()) for s in self._schemata])

    def _methods(self):
        return set.union(*[set(s.getMethods()) for s in self._schemata])

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

    def set_hosts(self, hosts=None):
        self._hosts = hosts
        del self.objects

    def set_class(self, klass, package='org.matahariproject'):
        self._class = klass
        self._package = package
        del self.objects

    def selected_classname(self):
        return '%s:%s' % (self._package, self._class)

    def list_hosts(self):
        return set(self.manager.hosts())

    def list_packages(self):
        return set(self._qmf.packages)

    def list_classnames(self, package=None):
        classes = list(self._qmf.classes)
        pkgmatch = (package is None and
                    (lambda k: True) or
                    (lambda k: k.getPackageName() == package))
        return set([k.getClassName() for k in classes if pkgmatch(k)])

    def agents(self):
        return set(self.manager.agents(self._hosts))
