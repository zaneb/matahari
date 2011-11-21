#!/usr/bin/env python

"""
  test_host_api.py - Copyright (c) 2011 Red Hat, Inc.
  Written by Dave Johnson <dajo@redhat.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the
  Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
"""

import commands as cmd
from qmf2 import QmfAgentException
import matahariTest as testUtil
import unittest
import time
import sys
import os

qmf = None
dbus = None

# Initialization
# =====================================================
def setUpModule():
    global connection, qmf, dbus
    connection = HostTestsSetup()
    qmf = connection.qmf
    dbus = connection.dbus

def tearDownModule():
    global connection
    connection.tearDown()

class HostTestsSetup(testUtil.TestsSetup):
    def __init__(self):
        testUtil.TestsSetup.__init__(self, "matahari-qmf-hostd", "host", "Host",
                                     "matahari-dbus-hostd", ("org.matahariproject.Host",
                                                             "/org/matahariproject/Host",
                                                             "org.matahariproject.Host"))


class HostApiTests(unittest.TestCase):

    # TEST - getProperties()
    # =====================================================
    def test_hostname_property(self):
        value = cmd.getoutput("hostname")

        qmf_value = qmf.props.get('hostname')
        self.assertEquals(qmf_value, value, "QMF hostname not matching")

        if testUtil.haveDBus:
            dbus_value = dbus.get('hostname')
            self.assertEquals(dbus_value, value, "DBus hostname not matching")

    def test_os_property(self):
        value = cmd.getoutput("uname -a | awk '{print $1,\"(\"$3\")\"}'")
        qmf_value = qmf.props.get('os')
        self.assertEquals(qmf_value, value, "QMF os not matching")

        if testUtil.haveDBus:
            dbus_value = dbus.get('os')
            self.assertEquals(dbus_value, value, "DBus os not matching")

    def test_arch_property(self):
        value = cmd.getoutput("uname -a | awk '{print $12}'")
        qmf_value = qmf.props.get('arch')
        self.assertEquals(qmf_value, value, "QMF os not matching")

        if testUtil.haveDBus:
            dbus_value = dbus.get('arch')
            self.assertEquals(dbus_value, value, "DBus arch not matching")

    def test_wordsize_property(self):
        qmf_value = qmf.props.get('wordsize')
        uname = cmd.getoutput("uname -a")
        word_size = 0
        if "i386" in uname or "i686" in uname:
            word_size = 32
        elif "x86_64" in uname or "s390x" in uname or "ppc64" in uname:
            word_size = 64
        self.assertEquals(qmf_value, word_size, "QMF wordsize not matching")

        if testUtil.haveDBus:
            dbus_value = dbus.get('wordsize')
            self.assertEquals(int(dbus_value), word_size, "DBus wordsize not matching")

    def test_memory_property(self):
        value = int(cmd.getoutput("free | grep Mem | awk '{ print $2 }'"))
        qmf_value = qmf.props.get('memory')
        self.assertEquals(qmf_value, value, "QMF memory not matching")

        if testUtil.haveDBus:
            dbus_value = dbus.get('memory')
            self.assertEquals(int(dbus_value), value, "DBus memory not matching")

    def test_swap_property(self):
        value = int(cmd.getoutput("free | grep Swap | awk '{ print $2 }'"))
        qmf_value = qmf.props.get('swap')
        self.assertEquals(qmf_value, value, "QMF swap not matching")

        if testUtil.haveDBus:
            dbus_value = dbus.get('swap')
            self.assertEquals(int(dbus_value), value, "DBus swap not matching")

    def test_cpu_count_property(self):
        qmf_value = qmf.props.get('cpu_count')
        uname = cmd.getoutput("uname -a")

        if "s390x" in uname:
            value = int(cmd.getoutput("cat /proc/cpuinfo | grep processors | awk '{ print $4 }'"))
        else:
            value = int(cmd.getoutput("cat /proc/cpuinfo | grep processor | wc -l"))
        self.assertEquals(qmf_value, value, "QMF cpu count not matching")

        if testUtil.haveDBus:
            dbus_value = dbus.get('cpu_count')
            self.assertEquals(int(dbus_value), value, "DBus cpu count not matching")

    def test_cpu_cores_property(self):
        # XXX Core count is still busted in F15, sigar needs to be updated
        if os.path.exists("/etc/fedora-release") and open("/etc/fedora-release", "r").read().strip() == "Fedora release 15 (Lovelock)":
            return
        qmf_value = str(qmf.props.get('cpu_cores'))

        if "s390x" in cmd.getoutput("uname -a"):
            core_count = cmd.getoutput("cat /proc/cpuinfo | grep processors | awk '{ print $4 }'").strip()
        else:
            core_count = cmd.getoutput("cat /proc/cpuinfo | grep \"core id\" | uniq | wc -l").strip()

        self.assertEquals(qmf_value, core_count, "QMF cpu core count ("+qmf_value+") not matching expected ("+core_count+")")

        if testUtil.haveDBus:
            dbus_value = str(dbus.get('cpu_cores'))
            self.assertEquals(dbus_value, core_count, "DBus cpu core count ("+dbus_value+") not matching expected ("+core_count+")")

    def test_cpu_model_property(self):
        qmf_value = qmf.props.get('cpu_model')
        uname = cmd.getoutput("uname -a")

        if "s390x" in uname:
            expected = cmd.getoutput("cat /proc/cpuinfo | grep 'processor 0' | awk -F, '{ print $3 }' | awk -F= '{ print $2 }'").strip()
            self.assertEquals(qmf_value, expected, "QMF cpu model ("+qmf_value+") not matching expected ("+expected+")")

            if testUtil.haveDBus:
                dbus_value = dbus.get('cpu_model')
                self.assertEquals(dbus_value, value, "DBus cpu model ("+dbus_value+") not matching expected ("+expected+")")

        elif "ppc64" in uname:
            expected = cmd.getoutput("cat /proc/cpuinfo | grep cpu | head -1")
            self.assertTrue(qmf_value in expected, "QMF cpu model ("+qmf_value+") not found in ("+expected+")")

            if testUtil.haveDBus:
                dbus_value = dbus.get('cpu_model')
                self.assertTrue(dbus_value in value, "DBus cpu model ("+dbus_value+") not found in ("+expected+")")

        else:
            expected = cmd.getoutput("cat /proc/cpuinfo | grep 'model name' | head -1 | awk -F: {'print $2'}").strip()
            self.assertEquals(qmf_value, expected, "QMF cpu model ("+qmf_value+") not matching expected ("+expected+")")

            if testUtil.haveDBus:
                dbus_value = dbus.get('cpu_model')
                self.assertEquals(dbus_value, expected, "DBus cpu model ("+dbus_value+") not matching expected ("+expected+")")

    def test_cpu_flags_property(self):
        qmf_value = qmf.props.get('cpu_flags').strip()
        uname = cmd.getoutput("uname -a")
        if "s390x" in uname:
            value = cmd.getoutput("cat /proc/cpuinfo | grep 'features' | head -1 | awk -F: {'print $2'}").strip()
            self.assertEquals(qmf_value, value, "QMF cpu flags not matching")

            if testUtil.haveDBus:
                dbus_value = dbus.get('cpu_flags').strip()
                self.assertEquals(dbus_value, value, "DBus cpu flags not matching")

        elif "ppc64" in uname:
            value = cmd.getoutput("cat /proc/cpuinfo | grep cpu | head -1")
            self.assertTrue(qmf_value in value, "QMF cpu flags not matching")

            if testUtil.haveDBus:
                dbus_value = dbus.get('cpu_flags').strip()
                self.assertTrue(dbus_value in value, "DBus cpu flags not matching")

        else:
            value = cmd.getoutput("cat /proc/cpuinfo | grep 'flags' | head -1 | awk -F: {'print $2'}").strip()
            self.assertEquals(qmf_value, value, "QMF cpu flags not matching")

            if testUtil.haveDBus:
                dbus_value = dbus.get('cpu_flags').strip()
                self.assertEquals(dbus_value, value, "DBus cpu flags not matching")

    def test_update_interval_property(self):
        value = qmf.props.get('update_interval')
        self.assertEquals(value, 5, "update interval not matching")

    def test_last_updated_property(self):
        value = qmf.props.get('last_updated')
        value = value / 1000000000
        now = int("%.0f" % time.time())
        delta = testUtil.getDelta(value, now)
        self.assertFalse(delta > 5, "last updated off gt 5 seconds")

    #def test_sequence_property(self):
    #     self.fail("no verification")

    def test_free_mem_property(self):
        qmf_value = qmf.props.get('free_mem')
        top_value = int(cmd.getoutput("free | grep Mem | awk '{ print $4 }'"))
        self.assertTrue(testUtil.checkTwoValuesInMargin(qmf_value, top_value, 0.05, "free memory"), "QMF free memory outside margin")

        if testUtil.haveDBus:
            dbus_value = dbus.get('free_mem')
            self.assertTrue(testUtil.checkTwoValuesInMargin(dbus_value, top_value, 0.05, "free memory"), "DBus free memory outside margin")

    def test_free_swap_property(self):
        qmf_value = qmf.props.get('free_swap')
        top_value = int(cmd.getoutput("free | grep Swap | awk '{ print $4 }'"))
        self.assertTrue(testUtil.checkTwoValuesInMargin(qmf_value, top_value, 0.05, "free swap"), "QMF free swap outside margin")

        if testUtil.haveDBus:
            dbus_value = dbus.get('free_swap')
            self.assertTrue(testUtil.checkTwoValuesInMargin(dbus_value, top_value, 0.05, "free swap"), "DBus free swap outside margin")

    # TEST - get_uuid()
    # =====================================================
    #def test_get_uuid_Hardware_lifetime(self):
    #    XXX requires dmidecode, which requires root
    #    result = qmf.get_uuid('Hardware')
    #    self.assertNotEqual(result.get('uuid'),'not-available', "not-available text not found on parm 'lifetime'")

    def test_get_uuid_Reboot_lifetime(self):
        qmf_result = qmf.get_uuid('Reboot').get('uuid')
        self.assertNotEqual(qmf_result, 'not-available', "QMF Reboot lifetime returned 'not-available'")

        if testUtil.haveDBus:
            dbus_result = dbus.get_uuid('Reboot')
            self.assertEqual(qmf_result, dbus_result, "uuid for 'Reboot' differ for QMF (%s) and DBus(%s)" % (qmf_result, dbus_result))

    def test_get_uuid_unset_Custom_lifetime(self):
        cmd.getoutput("rm -rf /etc/custom-machine-id")
        tearDownModule()
        setUpModule()
        result = qmf.get_uuid('Custom')
        self.assertEqual(result.get('uuid'), 'not-available', "QMF unset Custom liftetime not returning 'not-available' ("+result.get('uuid')+")")

        if testUtil.haveDBus:
            dbus_result = str(dbus.get_uuid('Custom'))
            self.assertEqual(dbus_result, 'not-available', "DBus unset Custom liftetime not returning 'not-available' ("+dbus_result+")")

    def test_get_uuid_unknown_lifetime(self):
        result = qmf.get_uuid('lifetime')
        self.assertEqual(result.get('uuid'), 'invalid-lifetime', "QMF parm 'lifetime' not returning 'invalid-lifetime' ("+result.get('uuid')+")")

        if testUtil.haveDBus:
            dbus_result = str(dbus.get_uuid('lifetime'))
            self.assertEqual(dbus_result, 'invalid-lifetime', "DBus parm 'lifetime' not returning 'invalid-lifetime' ("+dbus_result+")")

    def test_get_uuid_empty_string(self):
        # UUID with empty lifetime should be same as 'filesystem'
        filesystem = qmf.get_uuid('filesystem').get('uuid')
        result = qmf.get_uuid('').get('uuid')
        self.assertEqual(result, filesystem, "QMF UUID with empty lifetime (%s) differs from 'filesystem' lifetime (%s)" % (result, filesystem))

        if testUtil.haveDBus:
            dbus_result = str(dbus.get_uuid(''))
            self.assertEqual(dbus_result, filesystem, "DBus UUID with empty lifetime (%s) differs from 'filesystem' lifetime (%s)" % (dbus_result, filesystem))

    def test_get_uuid_zero_parameters(self):
        self.assertRaises(Exception, qmf.get_uuid)

        if testUtil.haveDBus:
            self.assertRaises(TypeError, dbus.get_uuid)

    # TEST - set_uuid()
    # =====================================================
    def test_set_uuid_Custom_lifetime(self):
        test_uuid = testUtil.getRandomKey(20)
        qmf.set_uuid('Custom', test_uuid)
        result = qmf.get_uuid('Custom')
        self.assertEqual(result.get('uuid'), test_uuid, "QMF uuid value ("+result.get('uuid')+") not matching expected("+test_uuid+")")
        connection.reQuery()
        self.assertEqual(qmf.props.get('custom_uuid'), test_uuid, "QMF uuid value ("+qmf.props.get('uuid')+") not matching expected("+test_uuid+")")

        if testUtil.haveDBus:
            test_uuid = testUtil.getRandomKey(20)
            dbus.set_uuid('Custom', test_uuid)
            uuid = dbus.get_uuid('Custom')
            self.assertEqual(uuid, test_uuid, "DBus uuid value ("+uuid+") not matching expected("+test_uuid+")")
            dbus.reQuery()
            uuid = str(dbus.get('custom_uuid'))
            self.assertEqual(uuid, test_uuid, "DBus uuid value ("+uuid+") not matching expected("+test_uuid+")")

    def test_set_uuid_new_Custom_lifetime(self):
        test_uuid = testUtil.getRandomKey(20)
        qmf.set_uuid('Custom', test_uuid)
        result = qmf.get_uuid('Custom')
        self.assertEqual(result.get('uuid'), test_uuid, "QMF uuid value ("+result.get('uuid')+") not matching expected("+test_uuid+")")
        connection.reQuery()
        self.assertEqual(qmf.props.get('custom_uuid'), test_uuid, "QMF uuid value ("+qmf.props.get('uuid')+") not matching expected("+test_uuid+")")

        if testUtil.haveDBus:
            test_uuid = testUtil.getRandomKey(20)
            dbus.set_uuid('Custom', test_uuid)
            uuid = dbus.get_uuid('Custom')
            self.assertEqual(uuid, test_uuid, "DBus uuid value ("+uuid+") not matching expected("+test_uuid+")")
            dbus.reQuery()
            uuid = str(dbus.get('custom_uuid'))
            self.assertEqual(uuid, test_uuid, "DBus uuid value ("+uuid+") not matching expected("+test_uuid+")")

    def test_set_uuid_Hardware_lifetime_fails(self):
        result = qmf.set_uuid('Hardware', testUtil.getRandomKey(20) )
        self.assertEqual(result.get('rc'), 23, "Unexpected return code ("+str(result.get('rc'))+"), expected 23")

    def test_set_uuid_Reboot_lifetime_fails(self):
        result = qmf.set_uuid('Reboot', testUtil.getRandomKey(20) )
        self.assertEqual(result.get('rc'), 23, "Unexpected return code ("+str(result.get('rc'))+"), expected 23")

    # TEST - misc
    # =====================================================
    #def test_identify(self):
    #    self.fail("no verification")

    #def test_reboot(self):
    #    self.fail("no verification")

    #def test_shutdown(self):
    #    self.fail("no verification")

