import os

import sys
import time
import matahariTest as testUtil
import unittest
#import commands as cmd
import cqpid
from qmf2 import QmfAgentException
import cqmf2
import dbus.service
import threading

qmf = None
qmf_dbus = None
connection = None

start_agent_and_broker = False

bus_name = 'org.matahariproject.Test'
object_path = "/org/matahariproject/Test"

class DBusTestObject(dbus.service.Object):
    """
    DBus object against which will the DBus Bridge be tested.
    """
    def __init__(self):
        self.config = "/etc/dbus-1/system.d/org.matahariproject.Test.conf"
        bus = dbus.SystemBus()
        # Create configuration file that allows to create object on systembus
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
            # Reload DBus configuration
            bus.call_blocking("org.freedesktop.DBus", "/", "org.freedesktop.DBus", "ReloadConfig", "", [])
            time.sleep(2)
        dbus_name = dbus.service.BusName(bus_name, bus=bus)
        dbus.service.Object.__init__(self, bus, object_path, dbus_name)
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

    @dbus.service.signal(dbus_interface='org.matahariproject.Test',
                         signature="s")
    def simpleSignal(self, s):
        print "simpleSignal", s

    @dbus.service.signal(dbus_interface='org.matahariproject.Test',
                         signature="biudsaiasa(is)a{is}")
    def complexSignal(self, b, i, u, d, s, a_i, a_s, a_is, d_is):
        print "complexSignal", b, i, u, d, s, a_i, a_s, a_is, d_is

    @dbus.service.method(dbus_interface='org.matahariproject.Test',
                         in_signature='s',
                         out_signature='')
    def emit_simpleSignal(self, s):
        print "Emitting signal simpleSignal(%s)" % s
        self.simpleSignal(s)

    @dbus.service.method(dbus_interface='org.matahariproject.Test',
                         in_signature='biudsaiasa(is)a{is}',
                         out_signature='')
    def emit_complexSignal(self, b, i, u, d, s, a_i, a_s, a_is, d_is):
        print "Emitting signal complexSignal ", b, i, u, d, s, a_i, a_s, a_is, d_is
        self.complexSignal(b, i, u, d, s, a_i, a_s, a_is, d_is)

class DBusThread(threading.Thread):
    """
    DBus server for testing will be ran in separate thread.
    """
    def run(self):
        # Start the main loop
        from dbus.mainloop.glib import DBusGMainLoop
        from gobject import MainLoop, threads_init
        threads_init()
        self.loop = DBusGMainLoop(set_as_default=True)
        self.obj = DBusTestObject()
        self.mainloop = MainLoop()
        self.mainloop.run()

    def stop(self):
        self.mainloop.quit()

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

class DBusBridgeTestsSetup(testUtil.TestsSetup):
    """ Connection to QMF broker. """
    def __init__(self):
        self.thread = DBusThread()
        self.thread.start()
        #testUtil.TestsSetup.__init__(self, "../../linux.build/src/dbus-bridge/matahari-qmf-dbus-bridged", "DBusBridge", "DBusBridge")
        testUtil.TestsSetup.__init__(self, "./matahari-qmf-dbus-bridged-wrapper", "DBusBridge", "DBusBridge")

    def tearDown(self):
        self.thread.stop()
        testUtil.TestsSetup.tearDown(self)

class DBusBridgeBasicApiTests(unittest.TestCase):
    """
    Class for executing basic tests of DBusBridge. Basic tests don't require
    permanent bridge to be created.
    """
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

        self.assertEquals(type(result[6]), bool, "Invalid type (%s) of seventh argument" % type(result[6]))
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

    def test_call_wrongBusName(self):
        self.assertRaises(QmfAgentException,
                qmf.call, "org.matahariproject.Test.Bad",
                "/org/matahariproject/Test", "org.matahariproject.Test",
                "method", [])

    def test_call_wrongObjectPath(self):
        self.assertRaises(QmfAgentException,
                qmf.call, "org.matahariproject.Test",
                "/org/matahariproject/Test/Bad", "org.matahariproject.Test",
                "method", [])

    def test_call_wrongInterface(self):
        self.assertRaises(QmfAgentException,
                qmf.call, "org.matahariproject.Test",
                "/org/matahariproject/Test", "org.matahariproject.Test.Bad",
                "method", [])

    def test_call_wrongMethod(self):
        self.assertRaises(QmfAgentException,
                qmf.call, "org.matahariproject.Test",
                "/org/matahariproject/Test", "org.matahariproject.Test",
                "BadMethod", [])

    def test_call_wrongArguments(self):
        self.assertRaises(QmfAgentException,
                qmf.call, "org.matahariproject.Test",
                "/org/matahariproject/Test", "org.matahariproject.Test",
                "multiplyString", [3])

        self.assertRaises(QmfAgentException,
                qmf.call, "org.matahariproject.Test",
                "/org/matahariproject/Test", "org.matahariproject.Test",
                "multiplyString", [3, "abc", "bad"])

    def test_object_wrongBusName(self):
        self.assertRaises(QmfAgentException,
            qmf.add_dbus_object, "org.matahariproject.Test.Bad",
            "/org/matahariproject/Test")

    def test_object_wrongObjectPath(self):
        self.assertRaises(QmfAgentException,
            qmf.add_dbus_object, "org.matahariproject.Test",
            "/org/matahariproject/Test/Bad")



class DBusBridgeAdvancedApiTests(unittest.TestCase):
    """
    Advanced tests for DBus Bridge. This tests will create permanent bridge.
    """
    @classmethod
    def setUpClass(cls):
        qmf.add_dbus_object("org.matahariproject.Test", "/org/matahariproject/Test")

        # Wait for dbus object to appear
        global qmf_dbus
        loop_count = 0
        found = False
        while not found and loop_count < 70:
            agents = connection.session.getAgents()
            sys.stderr.write("Agents\n")
            for agent in agents:
                if "DBusBridge" in str(agent):
                    sys.stderr.write("Agent: %s\n" % str(agent))
                    sys.stderr.write("query: {class:'%s',package:'%s'}\n" % (object_path, bus_name))
                    objs = agent.query("{class:'%s',package:'%s'}" % (object_path, bus_name))
                    if objs and len(objs):
                        qmf_dbus = objs[0]
                        found = True
                        break
            time.sleep(1)
            loop_count = loop_count + 1

    def test_object_call_wrongInterface(self):
        self.assertTrue(qmf_dbus is not None, "Bridged DBus object hasn't been created")

        self.assertTrue(not "org.matahariproject.Test.Bad.testComplexArgs" in dir(qmf_dbus))

    def test_object_call_wrongMethod(self):
        self.assertTrue(qmf_dbus is not None, "Bridged DBus object hasn't been created")

        self.assertTrue(not "org.matahariproject.Test.badMethod" in dir(qmf_dbus))

    def test_object_call_testComplexArgs(self):
        self.assertTrue(qmf_dbus is not None, "Bridged DBus object hasn't been created")

        s = [[10, "test", ["a", "b", "c"], [[1, [2, 3]], [4, [5, 6, 7]]]],
             [10, "test", ["a", "b", "c"], [[1, [2, 3]], [4, [5, 6, 7]]]]]

        result = qmf_dbus.__getattr__("org.matahariproject.Test.testComplexArgs")(s)
        self.assertEquals(result['1'], s, "Received array (%s) differs from excepted array (%s)" % (str(result), str(s)))

    def test_object_signal_simpleSignal(self):
        self.assertTrue(qmf_dbus is not None, "Bridged DBus object hasn't been created")
        print "Waiting for signal"
        qmf_dbus.__getattr__("org.matahariproject.Test.emit_simpleSignal")("test")
        time.sleep(2)

        # Wait for event that corresponds with the signal
        event = waitForSignal(connection.session)
        # Check is signal was received
        self.assertTrue(event is not None, "Signal has not been received")
        # Check signal data
        self.assertEquals(event.getData(0).getProperty('s'), 'test', "Value obtained from signal doesn't match sent value")

    def test_object_signal_complexSignal(self):
        self.assertTrue(qmf_dbus is not None, "Bridged DBus object hasn't been created")
        print "Waiting for signal"
        # signature = "biudsaiasa(is)a{is}")
        args = (True, -42, 42, 3.14, "test", [1, 2, 3], ["h", "e", "ll", "o"], [[1, "a"], [2, "b"]], [[1, 'a'], [2, 'b'], [3, 'c']])
        qmf_dbus.__getattr__("org.matahariproject.Test.emit_complexSignal")(*args)
        time.sleep(2)

        # Wait for event that corresponds with the signal
        event = waitForSignal(connection.session)
        # Check is signal was received
        self.assertTrue(event is not None, "Signal has not been received")
        # Check signal data
        data = event.getData(0)
        for i, name in enumerate(("b", "i", "u", "d", "s", "a_i", "a_s", "a_is", "d_is")):
            self.assertEquals(data.getProperty(name), args[i],
                    "Value of argument '%s' obtained from signal (%s) doesn't match sent value (%s)" %
                    (name, data.getProperty(name), args[i]))

def waitForSignal(session):
    # Wait up to 10 seconds for signal
    event = cqmf2.ConsoleEvent()
    while session._impl.nextEvent(event, cqpid.Duration(10000)):
        if event.getType() == cqmf2.CONSOLE_EVENT and event.getAgent().getAttributes()['_product'] == 'DBusBridge':
            return event
    return None
