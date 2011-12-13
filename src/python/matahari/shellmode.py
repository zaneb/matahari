"""
Matahari Shell modes.
"""

from interpreter import *
import shelltypes


class RootMode(Mode):
    """The default, root, shell mode."""

    def __init__(self, manager):
        Mode.__init__(self)
        self.manager = manager

        select_wrapper = Command('select', 'host',
                                 [shelltypes.Host(self.manager)])
        self += select_wrapper(self.select)

    def getFilteredMode(self, hosts):
        return FilteredMode(self.manager, hosts)

    def getClassMode(self, package, klass):
        return ClassMode(self.manager, package, klass)

    @Command('show', 'hosts')
    def show_hosts(self, kw_show, kw_hosts):
        """Show the list of connected hosts."""
        self.write(*[str(h) for h in self.manager.hosts()])

    def select(self, kw_select, kw_host, *arg_hosts):
        """ Select a set of hosts to restrict commands to."""
        self.shell.set_mode(self.getFilteredMode(arg_hosts))

    @Command('class', ('package', 'PACKAGE'), 'CLASS')
    def package_class(self, kw_class, (kw_package, package), klass):
        """ Select an object class to operate on."""
        if kw_package is None:
            package = 'org.matahariproject'
        self.shell.set_mode(self.getClassMode(package, klass))


class FilteredMode(RootMode):
    """Shell mode filtered by host"""

    def __init__(self, manager, hosts):
        self.hosts = hosts
        RootMode.__init__(self, manager)

    def getUnfilteredMode(self):
        return RootMode(self.manager)

    def getClassMode(self, package, klass):
        return FilteredClassMode(self.manager, self.hosts, package, klass)

    @Command('clear', 'selection')
    def clear_selection(self, kw_clear, kw_selection):
        """Clear the current host selection."""
        self.shell.set_mode(self.getUnfilteredMode())

    @Command('show', 'selection')
    def show_selection(self, *kws):
        """Show the current host selection."""
        self.write(*[str(h) for h in self.hosts])

    @Command('show', 'agents')
    def show_agents(self, *kws):
        """Show the list of connected agents."""
        self.write(*[str(a) for a in self.manager.agents(self.hosts)])

    def prompt(self):
        return '(host)'


class ClassMode(RootMode):
    """Shell mode filtered by QMF class."""

    def __init__(self, manager, package, klass):
        RootMode.__init__(self, manager)
        self.package = package
        self.klass = klass

    def getFilteredMode(self, hosts):
        return FilteredClassMode(self.manager, hosts, self.package, self.klass)

    def getUnclassifiedMode(self):
        return RootMode(self.manager)

    @Command('clear', 'class')
    def clear_class(self, kw_clear, kw_class):
        self.shell.set_mode(self.getUnclassifiedMode())

    @Command('select', 'property', 'PROPERTY', 'VALUE')
    def select_property(self, kw_select, kw_property, arg_property, arg_value):
        pass

    def prompt(self):
        return '[%s.%s]' % (self.package, self.klass)


class FilteredClassMode(FilteredMode, ClassMode):
    """Shell mode filtered by host and QMF class."""

    def __init__(self, manager, hosts, package, klass):
        FilteredMode.__init__(self, manager, hosts)
        ClassMode.__init__(self, manager, package, klass)

    def getUnfilteredMode(self):
        return ClassMode(self.manager, self.package, self.klass)

    def getUnclassifiedMode(self):
        return FilteredMode(self.manager, self.hosts)

    def prompt(self):
        return FilteredMode.prompt(self) + ClassMode.prompt(self)

