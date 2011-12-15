
class Host(object):
    def __init__(self, state):
        self.manager = state.manager

    def __call__(self, param):
        for h in self.manager.hosts():
            if str(h) == param:
                return h
        raise ValueError('Host "%s" not found' % param)

    def complete(self, param):
        return filter(lambda s: s.startswith(param),
                      map(str, self.manager.hosts()))

class SharedInfo(object):
    def __init__(self, default=None):
        self.last = self.default = default

    def set(self, last):
        self.last = last

    def setdefault(self):
        self.last = self.default

    def get(self):
        last, self.last = self.last, self.default
        return last

class Package(object):
    def __init__(self, state, info):
        self.state = state
        self.info = info
        self.info.setdefault()

    def __call__(self, param):
        if param not in self.state.list_packages():
            self.info.setdefault()
            raise ValueError('Package "%s" not found' % param)
        self.info.set(param)
        return param

    def default(self):
        self.info.setdefault()
        return self.info.default

    def complete(self, param):
        packages = self.state.list_packages()
        if param in packages:
            self.info.set(param)
        else:
            self.info.setdefault()
        return [p + ' ' for p in packages if p.startswith(param)]

class Class(object):
    def __init__(self, state, pkginfo):
        self.state = state
        self.pkginfo = pkginfo

    def __call__(self, param):
        package = self.pkginfo.get()
        if param not in self.state.list_classnames(package):
            fq = package is not None and ('%s:%s' % (package, param)) or param
            raise ValueError('Class "%s" not found' % fq)
        return param

    def complete(self, param):
        classnames = self.state.list_classnames(self.pkginfo.get())
        return [c + ' ' for c in classnames if c.startswith(param)]
