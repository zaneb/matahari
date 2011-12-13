"""
Matahari Shell modes.
"""

from interpreter import *
from core import Manager


class RootMode(Mode):
    """The default, root, shell mode."""
    def __init__(self, manager):
        self.manager = manager
        super(RootMode, self).__init__()

    def getFilteredMode(self, hosts):
        return FilteredMode(self.manager, hosts)

    def getClassMode(self, package, klass):
        return ClassMode(self.manager, package, klass)

    @Command('show', 'hosts')
    def show_hosts(self, kw_show, kw_hosts):
        """ shows connected hosts """
        self.write(*[str(h) for h in self.manager.hosts()])

    @Command('select', 'host', ['HOST'])
    def select(self, kw_host, *arg_hosts):
        """ Select host based on existing hostnames """
        hosts = [h for h in self.manager.hosts() if str(h) in arg_hosts]
        self.shell.set_mode(self.getFilteredMode(hosts))

    @Command('class', ('package', 'PACKAGE'), 'CLASS')
    def package_class(self, kw_class, (kw_package, package), klass):
        if kw_package is None:
            package = 'org.matahariproject'
        self.shell.set_mode(self.getClassMode(package, klass))


class FilteredMode(RootMode):
    """Shell mode filtered by host"""

    def __init__(self, manager, hosts):
        self.selected_hosts = hosts
        super(FilteredMode, self).__init__(manager)

    @Command('clear', 'selection')
    def clear_selection(self, kw_clear, kw_selection):
        pass

    @Command('show', 'selection')
    def show_selection(self, *kws):
        self.write(*[str(h) for h in self.selected_hosts])

    @Command('show', 'agents')
    def show_agents(self, *kws):
        self.write(*[str(a) for a in self.manager.agents(self.selected_hosts)])

    def prompt(self):
        return ' (host)'


class ClassMode(RootMode):
    """Shell mode filtered by QMF class."""

    def __init__(self, manager, package, klass):
        self.package = package
        self.klass = klass
        super(ClassMode, self).__init__(manager)

    @Command('clear', 'class')
    def clear_class(self, kw_clear, kw_class):
        self.shell.set_mode(RootMode())

    @Command('select', 'property', 'PROPERTY', 'VALUE')
    def select_property(self, kw_select, kw_property, arg_property, arg_value):
        pass

    def prompt(self):
        return ' [%s.%s]' % (self.package, self.klass)


class FilteredClassMode(FilteredMode, ClassMode):
    """Shell mode filtered by host and QMF class."""
    pass
