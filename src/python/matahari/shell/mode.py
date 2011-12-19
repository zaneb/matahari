"""
Matahari Shell modes.
"""

from interpreter import *
import types


QMF_ACCESS_MODES = (READ_CREATE, READ_WRITE, READ_ONLY) = range(1, 4)


class RootMode(Mode):
    """The default, root, shell mode."""

    def __init__(self, state):
        Mode.__init__(self)
        self.state = state

        select_wrapper = Command('select', 'host',
                                 [types.Host(self.state)])
        self += select_wrapper(self.select)

        pkg_state = types.SharedInfo('org.matahariproject')
        class_wrapper = Command('class',
                                ('package',
                                 types.Package(self.state, pkg_state)),
                                types.Class(self.state, pkg_state))
        self += class_wrapper(self.package_class)

    def writelist(self, objects):
        self.write(*[str(o) for o in objects])

    def getFilteredMode(self):
        return FilteredMode(self.state)

    def getClassMode(self):
        return ClassMode(self.state)

    @Command('show', 'hosts')
    def show_hosts(self, *kws):
        """Show the list of connected hosts."""
        self.writelist(self.state.list_hosts())

    @Command('show', 'agents')
    def show_agents(self, *kws):
        """Show the list of connected agents."""
        self.writelist(self.state.agents())

    def select(self, kw_select, kw_host, *arg_hosts):
        """ Select a set of hosts to restrict commands to."""
        self.state.set_hosts(arg_hosts)
        self.shell.set_mode(self.getFilteredMode())

    def package_class(self, kw_class, (kw_package, package), klass):
        """ Select an object class to operate on."""
        self.state.set_class(klass, package)
        self.shell.set_mode(self.getClassMode())


class FilteredMode(RootMode):
    """Shell mode filtered by host"""

    def __init__(self, state):
        RootMode.__init__(self, state)

    def getUnfilteredMode(self):
        return RootMode(self.state)

    def getClassMode(self):
        return FilteredClassMode(self.state)

    @Command('clear', 'selection')
    def clear_selection(self, kw_clear, kw_selection):
        """Clear the current host selection."""
        self.state.set_hosts(None)
        self.shell.set_mode(self.getUnfilteredMode())

    @Command('show', 'selection')
    def show_selection(self, *kws):
        """Show the current host selection."""
        self.writelist(self.state.hosts)

    def prompt(self):
        return '(host)'


class CallMethodHandler(object):
    """CLI Handler for a method call"""

    def __new__(cls, mode, method):
        handler = super(CallMethodHandler, cls).__new__(cls, mode, method)
        CallMethodHandler.__init__(handler, mode, method)
        wrapper = Command('call', handler._method.name, *handler._args)
        return wrapper(handler)

    def __init__(self, mode, method):
        self._mode = mode
        self._method = method
        self._args = [types.QMFProperty(a) for a in self._method.arguments
                                      if 'I' in a.dir]
        self.__name__ = method.name
        self.__doc__ = self.help()

    def help(self):
        lines = []
        if self._method.desc is not None:
            lines.append(self._method.desc)
        if self._args:
            lines.append('Parameters:')
            lines.extend(a.help() for a in self._args)
        return '\n'.join(lines)

    def __call__(self, kw_call, methodname, *args):
        """Call a method on the selected objects"""
        qmf_method = getattr(self._mode.state.objects, methodname)
        self._mode.writelist(qmf_method(*args))

class GetPropertyHandler(object):
    """CLI handler for getting an object property or statistic"""

    def __new__(cls, mode, prop, is_stat=False):
        handler = super(GetPropertyHandler, cls).__new__(cls, mode,
                                                         prop, is_stat)
        GetPropertyHandler.__init__(handler, mode, prop, is_stat)
        wrapper = Command('get', repr(prop))
        return wrapper(handler)

    def __init__(self, mode, prop, is_stat=False):
        self._mode = mode
        self._prop = prop
        self._is_stat = is_stat
        self.__name__ = 'get_property'
        self.__doc__ = self.help()

    def __call__(self, kw_get, propname):
        self._mode.writelist(getattr(self._mode.state.objects, propname))

    def help(self):
        doc = "Get the value of the %s %s for the selected objects"
        desc = self._prop.desc and '\n\n' + self._prop.desc or ''
        return (doc % (self._prop.name,
                       self._is_stat and 'statistic' or 'property')) + desc

class SetPropertyHandler(object):
    """CLI handler for setting an object property"""

    def __new__(cls, mode, prop):
        handler = super(SetPropertyHandler, cls).__new__(cls, mode, prop)
        SetPropertyHandler.__init__(handler, mode, prop)
        wrapper = Command('set', repr(prop), handler._arg)
        return wrapper(handler)

    def __init__(self, mode, prop):
        self._mode = mode
        self._prop = prop
        self._arg = types.QMFProperty(prop)
        self.__name__ = 'set_property'
        self.__doc__ = self.help()

    def __call__(self, kw_set, propname, value):
        setattr(self._mode.state.objects, propname, value)

    def help(self):
        doc = "Set the value of the %s property for the selected objects"
        return (doc % (str(self._arg),)) + '\nDetails:\n' + self._arg.help()


class ClassMode(RootMode):
    """Shell mode filtered by QMF class."""

    def __init__(self, state):
        RootMode.__init__(self, state)

        for prop in self.state.objects._properties():
            self += GetPropertyHandler(self, prop)

            if int(prop.access) == READ_WRITE:
                self += SetPropertyHandler(self, prop)

        for stat in self.state.objects._statistics():
            self += GetPropertyHandler(self, stat, True)

        for method in self.state.objects._methods():
            self += CallMethodHandler(self, method)

    def getFilteredMode(self):
        return FilteredClassMode(self.state)

    def getUnclassifiedMode(self):
        return RootMode(self.state)

    @Command('clear', 'class')
    def clear_class(self, kw_clear, kw_class):
        self.state.set_class(None, None)
        self.shell.set_mode(self.getUnclassifiedMode())

    @Command('select', 'property', 'PROPERTY', 'VALUE')
    def select_property(self, kw_select, kw_property, arg_property, arg_value):
        pass

    def prompt(self):
        return '[%s]' % (self.state.selected_classname(),)


class FilteredClassMode(FilteredMode, ClassMode):
    """Shell mode filtered by host and QMF class."""

    def __init__(self, state):
        FilteredMode.__init__(self, state)
        ClassMode.__init__(self, state)

    def getUnfilteredMode(self):
        return ClassMode(self.state)

    def getUnclassifiedMode(self):
        return FilteredMode(self.state)

    def prompt(self):
        return FilteredMode.prompt(self) + ClassMode.prompt(self)

