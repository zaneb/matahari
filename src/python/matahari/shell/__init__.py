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


import matahari.api
import mode
import interpreter


class MatahariShell(object):
    """A Matahari Shell object, with its own API state."""

    def __init__(self, connection=None, debug=False):
        """
        Initialise with optional connection parameters.
        Specify debug=True to print all exceptions.
        """
        args = []
        if connection is not None:
            args.append(connection)
        self.state = matahari.api.Matahari(*args)
        initial_mode = mode.RootMode(self.state)
        self.interpreter = interpreter.Interpreter('mhsh', initial_mode,
                                                   debug=debug)

    def __call__(self, *args):
        """
        Run the shell, either in interactive mode or executing the specified
        scripts.
        """
        def getscript(arg):
            if arg == '-':
                import sys
                return sys.stdin
            return file(arg)

        if args:
            for arg in args:
                with getscript(arg) as script:
                    self.interpreter.runscript(script)
        else:
            self.interpreter.cmdloop()
