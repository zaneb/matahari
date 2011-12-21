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

"""
Module for defining interpreter shell modes.

To define a new mode, create a subclass of the Mode class. Customise the
shell prompt by overriding the prompt() method. Add commands to the mode by
concatenating them with the += operator.
"""

import types
import time
import command


class Mode(object):
    """
    A interpreter shell mode. Each mode may have a distinctive prompt and its
    own set of commands, and may in addition store whatever state it likes.
    """

    def __init__(self, *commands):
        """Initialise with an initial set of commands."""

        # Avoid double initialisation
        if hasattr(self, '__initialised'):
            return
        self.__initialised = True

        self.shell = None
        self.commands = {}
        for m in dir(type(self)):
            try:
                c = getattr(self, m)
            except AttributeError:
                pass
            else:
                if (isinstance(c, command.CommandHandler)):
                    self += c
        for c in commands:
            self += c

    def activate(self, interpreter):
        """Method called by the interpreter when the mode becomes active."""
        self.shell = interpreter

    def deactivate(self, interpreter):
        """Method called by the interpreter when the mode becomes inactive."""
        if self.shell == interpreter:
            self.shell = None

    def prompt(self):
        """
        Return any additions to the command prompt that are to be appended in
        this mode. This method may be overridden by subclasses.
        """
        return ''

    def write(self, *lines):
        """Write one or more lines to the console."""
        for line in lines:
            self.shell.stdout.write(line + '\n')

    def __iadd__(self, cmd):
        """Add a command."""
        n = cmd.name
        c = self.commands.get(n)
        self.commands[n] = c + cmd
        return self

    def __getitem__(self, key):
        return self.commands[key]

    def __iter__(self):
        return iter(self.commands)

    def iterkeys(self):
        return iter(self)

    @command.Command('sleep', float)
    def sleep(self, kw_sleep, seconds):
        """Sleep for the specified number of seconds"""
        time.sleep(seconds)

    @command.Command('quit')
    def quit(self, kw_quit):
        """Exit the interpreter"""
        return True
