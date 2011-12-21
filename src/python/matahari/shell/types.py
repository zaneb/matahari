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

import uuid

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


QMF_TYPES = (U8, U16, U32, U64,
             _UNUSED,
             SSTR, LSTR,
             ABSTIME, DELTATIME,
             REF,
             BOOL,
             FLOAT, DOUBLE,
             UUID,
             FTABLE,
             S8, S16, S32, S64,
             OBJECT, LIST, ARRAY) = range(1, 23)

class QMFProperty(object):
    """CLI verification class for a QMF Property argument"""

    def __new__(cls, arg):
        """Create a CLI argument of the correct type from a SchemaArg."""
        if cls == QMFProperty:
            return _qmfPropertyTypes.get(arg.type, String)(arg)
        else:
            return super(QMFProperty, cls).__new__(cls, arg)

    def __init__(self, arg):
        self.arg = arg
        self.__name__ = str(self)

    def __repr__(self):
        return self.__class__.__name__.lower()

    def __str__(self):
        return self.arg.name

    def help(self):
        return ('%9s %-18s %s' % (repr(self),
                                  str(self),
                                  self.arg.desc or '')).rstrip()

    def default(self):
        return self.arg.default

class String(QMFProperty):
    """A QMF String property argument"""
    def __repr__(self):
        if self.arg.type not in _qmfPropertyTypes:
            return str(self)
        return self.arg.type == SSTR and 'sstr' or 'lstr'

    def __call__(self, param):
        maxlen = self.arg.maxlen or (self.arg.type == SSTR and 255) or 65535
        if len(param) > maxlen:
            raise ValueError('Parameter is too long')
        return param

class Bool(QMFProperty):
    """A QMF Boolean property argument"""

    TRUTHY = ('true',  't', '1', 'y', 'yes')
    FALSEY = ('false', 'f', '0', 'n', 'no')

    def __call__(self, param):
        lc = param.lower()
        if lc in self.TRUTHY:
            return True
        if lc in self.FALSEY:
            return False

        try:
            value = eval(param, {}, {})
        except (NameError, SyntaxError):
            raise ValueError('"%s" is not a boolean' % (param,))

        if not isinstance(value, (int, bool)):
            raise ValueError('"%s" is not a boolean' % (param,))

        return bool(value)

    def complete(self, param):
        if not param:
            return map(lambda l: l[0].capitalize(), (self.TRUTHY, self.FALSEY))
        lc = param.lower()
        matches = []
        for v in self.TRUTHY + self.FALSEY:
            if v == lc:
                return [param + ' ']
            if v.startswith(lc):
                return [param + v[len(param):] + ' ']
        return []

class Int(QMFProperty):
    """A QMF Integer property argument"""

    NAMES = {U8: 'u8', U16: 'u16', U32: 'u32', U64: 'u64',
             S8: 's8', S16: 's16', S32: 's32', S64: 's64'}
    MINIMUM = {U8:           0,     U16:          0,
               U32:          0,     U64:          0,
               S8:  -(1 <<  7),     S16: -(1 << 15),
               S32: -(1 << 31),     S64: -(1 << 63)}
    MAXIMUM = {U8:   (1 <<  8) - 1, U16:  (1 << 16) - 1,
               U32:  (1 << 32) - 1, U64:  (1 << 64) - 1,
               S8:   (1 <<  7) - 1, S16:  (1 << 15) - 1,
               S32:  (1 << 31) - 1, S64:  (1 << 63) - 1}

    def __str__(self):
        if self.arg.min is not None or self.arg.max is not None:
            return '<%d-%d>' % (self._min(), self._max())

        return QMFProperty.__str__(self)

    def __repr__(self):
        try:
            return self.NAMES[self.arg.type]
        except KeyError:
            return QMFProperty.__repr__(self)

    def _min(self):
        """Get the minimum allowed value"""
        if self.arg.min is not None:
            return self.arg.min
        try:
            return self.MINIMUM[self.arg.type]
        except KeyError:
            return -(1 << 31)

    def _max(self):
        """Get the maximum allowed value"""
        if self.arg.max is not None:
            return self.arg.max
        try:
            return self.MAXIMUM[self.arg.type]
        except KeyError:
            return (1 << 31) - 1

    def __call__(self, param):
        value = int(param)
        if value < self._min():
            raise ValueError('Value %d underflows minimum of range (%d)' %
                             (value, self._min()))
        if value > self._max():
            raise ValueError('Value %d overflows maximum of range (%d)' %
                             (value, self._max()))
        return value

class Float(QMFProperty):
    """A QMF Floating Point property argument"""

    def __repr__(self):
        return self.arg.type == FLOAT and 'float' or 'double'

    def __call__(self, param):
        return float(param)

class Uuid(QMFProperty):
    """A QMF UUID property argument"""

    LENGTH = 32

    def __call__(self, param):
        return uuid.UUID(param)

    def complete(self, param):
        raw = param.replace('-', '')
        try:
            val = int(param, 16)
        except ValueError:
            return []
        if len(raw) in (8, 12, 16, 20):
            return [param + '-']
        if len(raw) == self.LENGTH:
            return [param + ' ']
        return ['']

class List(QMFProperty):
    """A QMF List property argument"""

    def __call__(self, param):
        try:
            l = eval(param, {}, {})
        except (NameError, SyntaxError):
            raise ValueError('"%s" is not a valid list' % (param,))
        if not isinstance(l, list):
            raise ValueError('"%s" is not a list' % (param,))
        return l

    def complete(self, param):
        if not param:
            return ['[']
        if not param.startswith('['):
            return []
        if param.endswith(']'):
            try:
                self(param)
            except ValueError:
                return []
            return [param + ' ']
        return ['']

class Map(QMFProperty):
    """A QMF Map property argument"""

    def __call__(self, param):
        try:
            l = eval(param, {}, {})
        except (NameError, SyntaxError):
            raise ValueError('"%s" is not a valid map' % (param,))
        if not isinstance(l, dict):
            raise ValueError('"%s" is not a map' % (param,))
        return dict((unicode(k), v) for k, v in l.items())

    def complete(self, param):
        if not param:
            return ['{']
        if not param.startswith('{'):
            return []
        if param.endswith('}'):
            try:
                self(param)
            except ValueError:
                return []
            return [param + ' ']
        return ['']

_qmfPropertyTypes = {
    U8: Int,
    U16: Int,
    U32: Int,
    U64: Int,
    SSTR: String,
    LSTR: String,
    BOOL: Bool,
    FLOAT: Float,
    DOUBLE: Float,
    UUID: Uuid,
    FTABLE: Map,
    S8: Int,
    S16: Int,
    S32: Int,
    S64: Int,
    LIST: List,
    ARRAY: List,
}
