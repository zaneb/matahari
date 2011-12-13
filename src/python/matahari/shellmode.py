"""
Matahari Shell modes.
"""

from interpreter import *
import shelltypes


class RootMode(Mode):
    """The default, root, shell mode."""

    def __init__(self, state):
        Mode.__init__(self)
        self.state = state

        select_wrapper = Command('select', 'host',
                                 [shelltypes.Host(self.state.manager)])
        self += select_wrapper(self.select)

    def getFilteredMode(self):
        return FilteredMode(self.state)

    def getClassMode(self):
        return ClassMode(self.state)

    @Command('show', 'hosts')
    def show_hosts(self, kw_show, kw_hosts):
        """Show the list of connected hosts."""
        self.write(*[str(h) for h in self.state.list_hosts()])

    @Command('show', 'agents')
    def show_agents(self, *kws):
        """Show the list of connected agents."""
        self.write(*[str(a) for a in self.state.agents()])

    def select(self, kw_select, kw_host, *arg_hosts):
        """ Select a set of hosts to restrict commands to."""
        self.state.set_hosts(arg_hosts)
        self.shell.set_mode(self.getFilteredMode())

    @Command('class', ('package', 'PACKAGE'), 'CLASS')
    def package_class(self, kw_class, (kw_package, package), klass):
        """ Select an object class to operate on."""
        if kw_package is None:
            package = 'org.matahariproject'
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
        self.write(*[str(h) for h in self.state.hosts])

    def prompt(self):
        return '(host)'


class ClassMode(RootMode):
    """Shell mode filtered by QMF class."""

    def __init__(self, state):
        RootMode.__init__(self, state)

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

