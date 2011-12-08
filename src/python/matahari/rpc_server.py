import sys, os
import json
import traceback

(PLUGIN_PATH,) = ('.',) # TODO: choose proper install path

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
        filenames = [os.path.splitext(f) for f in os.listdir(path)]
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
