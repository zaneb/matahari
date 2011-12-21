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

import sys, os
import json
import traceback

(PLUGIN_PATH,) = ('/usr/lib/matahari/plugins',)

class Plugin(object):
    def __init__(self, module):
        self.module = module
        self.name = self.module.__name__

    def procedures(self):
        isProcedure = lambda a: callable(getattr(self.module, a))
        return [o for o in dir(self.module) if isProcedure(o) and
                                               not o.startswith('_')]

    def call(self, procedure, args_json=[], kwargs_json=[]):
        func = getattr(self.module, procedure)
        args = [json.loads(a) for a in args_json]
        kwargs = dict((k, json.loads(v)) for k,v in kwargs_json)
        result = func(*args, **kwargs)
        return json.dumps(result)

def formatException(exc_type, exc_val, exc_tb):
    return exc_type.__name__, str(exc_val), ''.join(traceback.format_tb(exc_tb))

def plugins(path=PLUGIN_PATH):
    old_path = sys.path
    try:
        sys.path = [path] + old_path
        try:
            filenames = [os.path.splitext(f) for f in os.listdir(path)]
        except OSError:
            return []
        modnames = [n for n, e in filenames if e == '.py']
        def getPlugin(name):
            try:
                mod = __import__(name, {}, {}, [], -1)
                return Plugin(mod)
            except ImportError:
                return None
        return filter(lambda p: p is not None, map(getPlugin, modnames))
    finally:
        sys.path = old_path


if __name__ == '__main__':
    print [p.name for p in plugins('src/rpc/plugins')]
