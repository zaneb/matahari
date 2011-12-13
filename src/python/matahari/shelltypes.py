
class Host(object):
    def __init__(self, manager):
        self.manager = manager

    def __call__(self, param):
        for h in self.manager.hosts():
            if str(h) == param:
                return h
        raise ValueError('Host "%s" not found' % param)

    def complete(self, param):
        return filter(lambda s: s.startswith(param),
                      map(str, self.manager.hosts()))

