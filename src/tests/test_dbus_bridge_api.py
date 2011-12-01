import os

import sys
import time
import matahariTest as testUtil
import unittest
import commands as cmd
import cqpid
from qmf2 import ConsoleSession
import dbus.service
import threading
qmf = None

start_agent_and_broker = True

class DBusTestObject(dbus.service.Object):
    def __init__(self):
        self.config = "/etc/dbus-1/system.d/org.matahariproject.Test.conf"
        if not os.path.exists(self.config):
            f = open(self.config, "w")
            f.write("""
<busconfig>
  <policy user="root">
    <allow own="org.matahariproject.Test"/>
  </policy>
  <policy context="default">
    <allow send_destination="org.matahariproject.Test"/>
  </policy>
</busconfig>
""")
            f.close()
            # TODO: dbus
            cmd.getoutput("qdbus --system org.freedesktop.DBus / org.freedesktop.DBus.ReloadConfig")
            time.sleep(2)
        bus = dbus.SystemBus()
        bus_name = dbus.service.BusName('org.matahariproject.Test', bus=bus)
        dbus.service.Object.__init__(self, bus, "/org/matahariproject/Test", bus_name)
        sys.stderr.write("Starting fake DBus API ...\n")

    def __del__(self):
        sys.stderr.write("Stopping fake DBus API\n")
        os.unlink(self.config)

    @dbus.service.method(dbus_interface='org.matahariproject.Test',
                         in_signature='is', out_signature='s')
    def multiplyString(self, i, s):
        return i * s

    @dbus.service.method(dbus_interface='org.matahariproject.Test',
                         in_signature='bixyutd', out_signature='dtuyxib')
    def testBasicTypes(self, b, i, x, y, u, t, d):
        # Just reverse the order
        return d, t, u, y, x, i, b

    @dbus.service.method(dbus_interface='org.matahariproject.Test',
                         in_signature='ai', out_signature='ai')
    def testSimpleArrayOfInt(self, l):
        l.reverse()
        return l

    @dbus.service.method(dbus_interface='org.matahariproject.Test',
                         in_signature='as', out_signature='as')
    def testSimpleArrayOfString(self, l):
        ll = [str(x).upper() for x in l]
        ll.reverse()
        return ll

    @dbus.service.method(dbus_interface='org.matahariproject.Test',
                         in_signature='(is)', out_signature='(si)')
    def testSimpleStruct(self, struct):
        i, s = struct
        return (s, i)


    @dbus.service.method(dbus_interface='org.matahariproject.Test',
                         in_signature= 'a{ss}a{is}a{s(ii)}a{sai}',
                         out_signature='a{ss}a{is}a{s(ii)}a{sai}')
    def testDict(self, d1, d2, d3, d4):
        return d1, d2, d3, d4

    @dbus.service.method(dbus_interface='org.matahariproject.Test',
                         in_signature= 'vava{sv}(vvv)',
                         out_signature='vava{sv}(vvv)')
    def testVariants(self, v, av, dsv, vvv):
        return v, av, dsv, vvv

    @dbus.service.method(dbus_interface='org.matahariproject.Test',
                         in_signature='a(isasa(iai))',
                         out_signature='a(isasa(iai))')
    def testComplexArgs(self, args):
        return args

class DBusThread(threading.Thread):
    def run(self):
        from dbus.mainloop.glib import DBusGMainLoop
        from gobject import MainLoop, threads_init
        threads_init()
        self.loop = DBusGMainLoop(set_as_default=True)
        self.obj = DBusTestObject()
        self.mainloop = MainLoop()
        self.mainloop.run()

    def stop(self):
        self.mainloop.quit()


class TestsSetup(object):
    def __init__(self, qmf_binary, agentKeyword, agentClass, dbus_binary=None, dbus_settings=[]):
        self.thread = DBusThread()
        self.thread.start()
        self.qmf_agent = None
        self.broker = None

        if start_agent_and_broker:
            self.broker = testUtil.MatahariBroker()
            self.broker.start()
            time.sleep(1)
            self.qmf_agent = testUtil.MatahariAgent(qmf_binary)
            self.qmf_agent.start()

        time.sleep(1)

        self._connectToBroker('localhost', '49001')

        self.agentKeyword = agentKeyword
        self.agentClass = agentClass
        self.qmf = self._findAgent(cmd.getoutput('hostname'))
        self.reQuery()

    def tearDown(self):
        self.thread.stop()
        self._disconnect()
        if self.qmf_agent is not None:
            self.qmf_agent.stop()
        if self.broker is not None:
            self.broker.stop()
        self.thread.join()

    def _disconnect(self):
        self.connection.close()
        self.session.close()

    def reQuery(self):
        self.qmf.update()
        self.qmf.props = self.qmf.getProperties()

    def _findAgent(self, hostname):
        loop_count = 0
        while loop_count < 70:
            agents = self.session.getAgents()
            sys.stderr.write("Agents: %s\n" % str(agents))
            for agent in agents:
                if self.agentKeyword in str(agent):
                    if agent.getAttributes().get('hostname') == hostname:
                        objs = agent.query("{class:" + self.agentClass + ",package:'org.matahariproject'}")
                        if objs and len(objs):
                            return objs[0]
            time.sleep(1)
            loop_count = loop_count + 1
        sys.exit("specific " + self.agentKeyword + " agent for " + hostname + " not found.")


    def _connectToBroker(self, hostname, port):
        self.connection = cqpid.Connection(hostname + ":" + port)
        self.connection.open()
        self.session = ConsoleSession(self.connection)
        self.session.open()


def sortList(l):
    """ Recursivly sort list. """
    for item in l:
        if isinstance(item, list):
            item.sort()
    l.sort()
    return l

# Initialization
# =====================================================
def setUpModule():
    global connection, qmf
    connection = DBusBridgeTestsSetup()
    qmf = connection.qmf

def tearDownModule():
    global connection
    connection.tearDown()

class DBusBridgeTestsSetup(TestsSetup):
    def __init__(self):
        TestsSetup.__init__(self, "../../linux.build/src/dbus-bridge/matahari-qmf-dbus-bridged", "DBusBridge", "DBusBridge")

class HostApiTests(unittest.TestCase):
    def test_call_multiplyString(self):
        result = qmf.call("org.matahariproject.Test",
                          "/org/matahariproject/Test",
                          "org.matahariproject.Test",
                          "multiplyString", [3, "abc"]).get("results")
        self.assertEquals(len(result), 1, "Invalid number of arguments")
        self.assertTrue(isinstance(result[0], basestring), "First and only argument is not string")
        self.assertEquals(result[0], "abcabcabc", "Result (%s) is not valid" % result)

    def test_call_testBasicTypes(self):
        result = qmf.call("org.matahariproject.Test",
                          "/org/matahariproject/Test",
                          "org.matahariproject.Test",
                          "testBasicTypes", [True, 1, 2L, 3, 4, 5L, 6.0]).get("results")
        self.assertEquals(len(result), 7, "Invalid number of arguments")

        self.assertEquals(type(result[0]), float, "Invalid type (%s) of first argument" % type(result[0]))
        self.assertEquals(result[0], 6.0, "Invalid value (%f) of first argument" % result[0])

        self.assertEquals(type(result[1]), long, "Invalid type (%s) of second argument" % type(result[1]))
        self.assertEquals(result[1], 5L, "Invalid value (%f) of second argument" % result[1])

        self.assertEquals(type(result[2]), int, "Invalid type (%s) of third argument" % type(result[2]))
        self.assertEquals(result[2], 4, "Invalid value (%f) of third argument" % result[2])

        self.assertEquals(type(result[3]), int, "Invalid type (%s) of fourth argument" % type(result[3]))
        self.assertEquals(result[3], 3, "Invalid value (%f) of fourth argument" % result[3])

        self.assertEquals(type(result[4]), long, "Invalid type (%s) of fifth argument" % type(result[4]))
        self.assertEquals(result[4], 2L, "Invalid value (%f) of fifth argument" % result[4])

        self.assertEquals(type(result[5]), int, "Invalid type (%s) of sixth argument" % type(result[5]))
        self.assertEquals(result[5], 1, "Invalid value (%f) of sixth argument" % result[5])

        self.assertEquals(type(result[6]), int, "Invalid type (%s) of seventh argument" % type(result[6]))
        self.assertEquals(result[6], True, "Invalid value (%f) of seventh argument" % result[6])

    def test_call_testSimpleArrayOfInt(self):
        l = [1, 2, 3, 4, 5]
        result = qmf.call("org.matahariproject.Test",
                          "/org/matahariproject/Test",
                          "org.matahariproject.Test",
                          "testSimpleArrayOfInt", [l]).get("results")
        self.assertEquals(len(result), 1, "Invalid number of arguments")
        l.reverse()
        self.assertEquals(result[0], l, "Received array (%s) differs from excepted array (%s)" % (result, l))

    def test_call_testSimpleArrayOfString(self):
        l = ["abc", "def", "ghi"]
        res = ["GHI", "DEF", "ABC"]
        result = qmf.call("org.matahariproject.Test",
                          "/org/matahariproject/Test",
                          "org.matahariproject.Test",
                          "testSimpleArrayOfString", [l]).get("results")
        self.assertEquals(len(result), 1, "Invalid number of arguments")
        self.assertEquals(result[0], res, "Received array (%s) differs from excepted array (%s)" % (result, res))

    def test_call_testSimpleStruct(self):
        s = [10, "test"]
        result = qmf.call("org.matahariproject.Test",
                          "/org/matahariproject/Test",
                          "org.matahariproject.Test",
                          "testSimpleStruct", [s]).get("results")
        self.assertEquals(len(result), 1, "Invalid number of arguments")
        self.assertEquals(result[0], ["test", 10], "Received struct (%s) differs from excepted struct (\"test\", 10)" % str(result))

    def test_call_testComplexArgs(self):
        # a(isasa(iai))
        s = [[10, "test", ["a", "b", "c"], [[1, [2, 3]], [4, [5, 6, 7]]]],
             [10, "test", ["a", "b", "c"], [[1, [2, 3]], [4, [5, 6, 7]]]]]
        result = qmf.call("org.matahariproject.Test",
                          "/org/matahariproject/Test",
                          "org.matahariproject.Test",
                          "testComplexArgs", [s]).get("results")
        self.assertEquals(len(result), 1, "Invalid number of arguments")
        self.assertEquals(result[0], s, "Received array (%s) differs from excepted array (%s)" % (str(result), str(s)))

    def test_call_testDict(self):
        # (a{ss}a{is}a{s(ii)}a{sai})
        s = [[["a", "aaa"], ["b", "bbb"], ["c", "ccc"]],
             [[1, "a"], [2, "b"]],
             [['a', [1, 1]], ['b', [1, 1]]],
             [['a', [1, 1, 1]], ['b', [1]]]]
        result = qmf.call("org.matahariproject.Test",
                          "/org/matahariproject/Test",
                          "org.matahariproject.Test",
                          "testDict", s).get("results")
        self.assertEquals(len(result), 4, "Invalid number of arguments")
        # DBus can switch order of the arguments, sort might be needed for other agrs as well
        result[0].sort()
        s[0].sort()
        self.assertEquals(result[0], s[0], "Received dict (%s) differs from excepted dict (%s)" % (str(result[0]), str(s[0])))
        self.assertEquals(result[1], s[1], "Received dict (%s) differs from excepted dict (%s)" % (str(result[1]), str(s[1])))
        self.assertEquals(result[2], s[2], "Received dict (%s) differs from excepted dict (%s)" % (str(result[2]), str(s[2])))
        self.assertEquals(result[3], s[3], "Received dict (%s) differs from excepted dict (%s)" % (str(result[3]), str(s[3])))

    def test_call_testVariants(self):
        # vava{sv}(vvv)
        s = [1, ["1", 3, -3, 4L, -4L, True], [["a", 1], ["b", "b"], ["c", False]], [1, "b", -3]]
        result = qmf.call("org.matahariproject.Test",
                          "/org/matahariproject/Test",
                          "org.matahariproject.Test",
                          "testVariants", s).get("results")
        self.assertEquals(len(result), 4, "Invalid number of arguments")
        self.assertEquals(sortList(result), sortList(s), "Received array (%s) differs from excepted array (%s)" % (str(result), str(s)))
