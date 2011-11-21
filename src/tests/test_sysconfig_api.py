#!/usr/bin/env python

"""
  test_sysconfig_api.py - Copyright (c) 2011 Red Hat, Inc.
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
import sys
from qmf2 import QmfAgentException
import time
from os import stat
import matahariTest as testUtil
from stat import *
from pwd import getpwuid
from grp import getgrgid
import unittest
import random
import string
import sys
import platform
import os
import threading
import SimpleHTTPServer
import SocketServer
import errno
from nose.plugins.attrib import attr


# The docs for SocketServer show an allow_reuse_address option, but I
# can't seem to make it work, so screw it, randomize the port.
HTTP_PORT = 49002 + random.randint(0, 500)

err = sys.stderr
testPuppetFile = "/sysconfig-puppet-test"
testAugeasFile = "/sysconfig-augeas-test"
testPath = "/tmp/sysconfig-test-http-root"
testPuppetFileWithPath = testPath + testPuppetFile
testAugeasFileWithPath = testPath + testAugeasFile
testPuppetFileUrl = ("http://127.0.0.1:%d" % HTTP_PORT) + testPuppetFile
testAugeasFileUrl = ("http://127.0.0.1:%d" % HTTP_PORT) + testAugeasFile
targetFilePerms = '440'
targetFileGroup = 'root'
targetFileOwner = 'root'
origFilePerms = '777'
origFileGroup = 'qpidd'
origFileOwner = 'qpidd'
puppetFileContents = "file { \""+testPuppetFileWithPath+"\":\n    owner => "+targetFileOwner+", group => "+targetFileGroup+", mode => "+targetFilePerms+"\n}"
augeasQuery = "/files/etc/mtab/1/spec"
augeasFileContents = "get %s\n" % augeasQuery
connection = None
qmf = None
dbus = None
httpd_thread = None

if testUtil.haveDBus:
    from dbus import DBusException

def resetTestFile(file, perms, owner, group, contents):
    cmd.getoutput("rm -rf " + file)
    cmd.getoutput("touch " + file)
    cmd.getoutput("chmod "+ perms +" "+ file)
    cmd.getoutput("chown "+ owner +":"+ group +" "+ file)
    testUtil.setFileContents(file, contents)
    #print "++checking test file pre-reqs++"
    if checkFile(file, perms, owner, group) != 0:
        sys.exit("problem setting up test file")
    #print "++DONE...checking test file pre-reqs++"

def wrapper(module, method, value, flag, schema, key):
    if schema == "augeas":
        resetTestFile(testAugeasFileWithPath, origFilePerms, origFileOwner, origFileGroup, augeasFileContents)
    else:
        resetTestFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup, puppetFileContents)
    results = None
    if method == 'uri':
        results = module.run_uri(value, flag, schema, key)
    elif method == 'string':
       results = module.run_string(value, flag, schema, key)
    return results

def checkFile(file, perms, owner, grp):
    count = 0
    if testUtil.getFilePermissionMask(file) != perms:
        err.write('\nfile permissions wrong, '+testUtil.getFilePermissionMask(file)+' != ' +perms+ '\n')
        count = count + 1
    if testUtil.getFileOwner(file) != owner:
        err.write('\nfile owner wrong, '+testUtil.getFileOwner(file)+' != ' +owner+ '\n')
        count = count + 1
    if testUtil.getFileGroup(file) != grp:
        err.write('\nfile group wrong, '+testUtil.getFileGroup(file)+' != ' +grp+ '\n')
        count = count + 1
    return count


class HTTPThread(threading.Thread):
    def run(self):
        try:
            os.makedirs(testPath)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise
        os.chdir(testPath)
        self.handler = SimpleHTTPServer.SimpleHTTPRequestHandler
        self.httpd = SocketServer.TCPServer(("", HTTP_PORT), self.handler)
        sys.stderr.write("Starting HTTP Server on port: %d ...\n" % HTTP_PORT)
        self.httpd.serve_forever()


# Initialization
# =====================================================
def setUpModule():
    global httpd_thread, connection, qmf, dbus
    httpd_thread = HTTPThread()
    httpd_thread.start()

    # get puppet pre-req
    if platform.dist()[0] == 'redhat':
        cmd.getoutput("wget -O /etc/yum.repos.d/rhel-aeolus.repo http://repos.fedorapeople.org/repos/aeolus/conductor/rhel-aeolus.repo")
        result = cmd.getstatusoutput("yum -y install puppet")
        if result[0] != 0:
            sys.exit("Unable to install puppet (required for sysconfig tests)")

    connection = SysconfigTestsSetup()
    qmf = connection.qmf
    dbus = connection.dbus

def tearDownModule():
    global connection
    connection.tearDown()
    httpd_thread.httpd.shutdown()
    httpd_thread.httpd.server_close()
    httpd_thread.join()

class SysconfigTestsSetup(testUtil.TestsSetup):
    def __init__(self):
        testUtil.TestsSetup.__init__(self, "matahari-qmf-sysconfigd", "Sysconfig", "Sysconfig",
                                           "matahari-dbus-sysconfigd", ("org.matahariproject.Sysconfig",
                                                                        "/org/matahariproject/Sysconfig",
                                                                        "org.matahariproject.Sysconfig"))

class TestSysconfigApi(unittest.TestCase):

    # TEST - getProperties()
    # =====================================================
    def test_hostname_property(self):
        value = cmd.getoutput("hostname")
        qmf_value = qmf.props.get('hostname')
        self.assertEquals(value, qmf_value, "QMF: hostname not expected")

        if testUtil.haveDBus:
            dbus_value = dbus.get('hostname')
            self.assertEquals(value, dbus_value, "DBus: hostname not expected")

    # TODO:
    #	no puppet
    #	duplicate keys
    #	flags
    #

    # TEST - run_uri()
    # ================================================================
    # TEST - Puppet
    def test_run_uri_good_url_puppet_manifest(self):
        results = wrapper(qmf, 'uri', testPuppetFileUrl, 0, 'puppet', testUtil.getRandomKey(5))
        self.assertTrue( results.get('status') == 'OK', "QMF result: " + str(results.get('status')) + " != OK")
        self.assertTrue( 0 == checkFile(testPuppetFileWithPath, targetFilePerms, targetFileOwner, targetFileGroup), "QMF: file properties not expected")

        if testUtil.haveDBus:
            results = wrapper(dbus, 'uri', testPuppetFileUrl, 0, 'puppet', testUtil.getRandomKey(5))
            self.assertTrue(str(results) == 'OK', "DBus result: " + str(results) + " != OK")
            self.assertTrue(0 == checkFile(testPuppetFileWithPath, targetFilePerms, targetFileOwner, targetFileGroup), "DBus: file properties not expected")

    def test_run_uri_http_url_not_found_puppet(self):
        self.assertRaises(QmfAgentException, wrapper, qmf, 'uri', testPuppetFileUrl+"_bad", 0, 'puppet', testUtil.getRandomKey(5))
        self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "QMF: file properties not expected")

        if testUtil.haveDBus:
            self.assertRaises(DBusException, wrapper, dbus, 'uri', testPuppetFileUrl+"_bad", 0, 'puppet', testUtil.getRandomKey(5))
            self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "DBus: file properties not expected")

    def test_run_uri_good_file_puppet_manifest(self):
        results = wrapper(qmf, 'uri', 'file://'+testPuppetFileWithPath, 0, 'puppet', testUtil.getRandomKey(5))
        self.assertTrue( results.get('status') == 'OK', "QMF: result: " + str(results.get('status')) + " != OK")
        self.assertTrue( 0 == checkFile(testPuppetFileWithPath, targetFilePerms, targetFileOwner, targetFileGroup), "QMF: file properties not expected")

        if testUtil.haveDBus:
            results = wrapper(dbus, 'uri', 'file://'+testPuppetFileWithPath, 0, 'puppet', testUtil.getRandomKey(5))
            self.assertTrue(str(results) == 'OK', "DBus: result: " + str(results) + " != OK")
            self.assertTrue(0 == checkFile(testPuppetFileWithPath, targetFilePerms, targetFileOwner, targetFileGroup), "DBus: file properties not expected")

    def test_run_uri_file_url_not_found(self):
        self.assertRaises(QmfAgentException, wrapper, qmf, 'uri', 'file://'+testPuppetFile, 0, 'puppet', testUtil.getRandomKey(5))
        self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "QMF: file properties not expected")

        if testUtil.haveDBus:
            self.assertRaises(DBusException, wrapper, dbus, 'uri', 'file://'+testPuppetFile, 0, 'puppet', testUtil.getRandomKey(5))
            self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "DBus: file properties not expected")

    def test_run_uri_bad_puppet_manifest(self):
        resetTestFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup, 'bad puppet script')
        results = qmf.run_uri(testPuppetFileUrl, 0, 'puppet', testUtil.getRandomKey(5))
        self.assertTrue( results.get('status') == 'FAILED\n1', "QMF: result: " + str(results.get('status')) + " != FAILED\n1")
        self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "QMF: file properties not expected")

        if testUtil.haveDBus:
            resetTestFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup, 'bad puppet script')
            results = dbus.run_uri(testPuppetFileUrl, 0, 'puppet', testUtil.getRandomKey(5))
            self.assertTrue( str(results) == 'FAILED\n1', "DBus: result: " + str(results) + " != FAILED\n1")
            self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "DBus: file properties not expected")

    def test_run_uri_non_schema(self):
        self.assertRaises(QmfAgentException, wrapper, qmf, 'uri', testPuppetFileUrl, 0, 'schema', testUtil.getRandomKey(5))
        self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "QMF: file properties not expected")

        if testUtil.haveDBus:
            self.assertRaises(DBusException, wrapper, dbus, 'uri', testPuppetFileUrl, 0, 'schema', testUtil.getRandomKey(5))
            self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "DBus: file properties not expected")

    # TODO: need to handle upstream vs rhel difference

    # TEST - Augeas
    @attr('augeas')
    def test_run_uri_good_url_augeas(self):
        results = wrapper(qmf, 'uri', testAugeasFileUrl, 0, 'augeas', testUtil.getRandomKey(5)).get('status')
        tokens = results.split('\n')
        self.assertEqual(tokens[0], 'OK', "QMF: result: %s != OK" % tokens[0])
        self.assertTrue(tokens[1].startswith("%s = " % augeasQuery), "QMF: wrong format of result (%s)" % tokens[1])

        if testUtil.haveDBus:
            results = wrapper(dbus, 'uri', testAugeasFileUrl, 0, 'augeas', testUtil.getRandomKey(5))
            tokens = results.split('\n')
            self.assertEqual(tokens[0], 'OK', "DBus: result: %s != OK" % tokens[0])
            self.assertTrue(tokens[1].startswith("%s = " % augeasQuery), "DBus: wrong format of result (%s)" % tokens[1])

    @attr('augeas')
    def test_run_uri_http_url_not_found_augeas(self):
        self.assertRaises(QmfAgentException, wrapper, qmf, 'uri', testAugeasFileUrl + "_bad", 0, 'augeas', testUtil.getRandomKey(5))

        if testUtil.haveDBus:
            self.assertRaises(DBusException, wrapper, dbus, 'uri', testAugeasFileUrl + "_bad", 0, 'augeas', testUtil.getRandomKey(5))

    @attr('augeas')
    def test_run_uri_good_file_augeas(self):
        results = wrapper(qmf, 'uri', 'file://'+testAugeasFileWithPath, 0, 'augeas', testUtil.getRandomKey(5)).get('status')
        tokens = results.split('\n')
        self.assertEqual(tokens[0], 'OK', "QMF: result: %s != OK" % tokens[0])
        self.assertTrue(tokens[1].startswith("%s = " % augeasQuery), "QMF: wrong format of result (%s)" % tokens[1])

        if testUtil.haveDBus:
            results = wrapper(dbus, 'uri', 'file://'+testAugeasFileWithPath, 0, 'augeas', testUtil.getRandomKey(5))
            tokens = results.split('\n')
            self.assertEqual(tokens[0], 'OK', "DBus: result: %s != OK" % tokens[0])
            self.assertTrue(tokens[1].startswith("%s = " % augeasQuery), "DBus: wrong format of result (%s)" % tokens[1])

    @attr('augeas')
    def test_run_uri_file_url_not_found(self):
        self.assertRaises(QmfAgentException, wrapper, qmf, 'uri', 'file://'+testAugeasFile, 0, 'augeas', testUtil.getRandomKey(5))

        if testUtil.haveDBus:
            self.assertRaises(DBusException, wrapper, dbus, 'uri', 'file://'+testAugeasFile, 0, 'augeas', testUtil.getRandomKey(5))

    def test_run_uri_empty_key(self):
        self.assertRaises(QmfAgentException, wrapper, qmf, 'uri', testPuppetFileUrl, 0, 'puppet', '')
        self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "QMF: file properties not expected")

        if testUtil.haveDBus:
            self.assertRaises(DBusException, wrapper, dbus, 'uri', testPuppetFileUrl, 0, 'puppet', '')
            self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "DBus: file properties not expected")

    def test_run_uri_special_chars_in_key(self):
        self.assertRaises(QmfAgentException, wrapper, qmf, 'uri', testPuppetFileUrl, 0, 'puppet', testUtil.getRandomKey(5) + "'s $$$")
        self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "QMF: file properties not expected")

        if testUtil.haveDBus:
            self.assertRaises(DBusException, wrapper, dbus, 'uri', testPuppetFileUrl, 0, 'puppet', testUtil.getRandomKey(5) + "'s $$$")
            self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "DBus: file properties not expected")

    # TEST - run_string()
    # ================================================================
    # TEST - Puppet
    def test_run_string_good_puppet_manifest(self):
        results = wrapper(qmf, 'string', testUtil.getFileContents(testPuppetFileWithPath), 0, 'puppet', testUtil.getRandomKey(5))
        self.assertTrue( results.get('status') == 'OK', "QMF: result: " + str(results.get('status')) + " != OK")
        self.assertTrue( 0 == checkFile(testPuppetFileWithPath, targetFilePerms, targetFileOwner, targetFileGroup), "QMF: file properties not expected")

        if testUtil.haveDBus:
            results = wrapper(dbus, 'string', testUtil.getFileContents(testPuppetFileWithPath), 0, 'puppet', testUtil.getRandomKey(5))
            self.assertTrue( results == 'OK', "DBus: result: " + str(results) + " != OK")
            self.assertTrue( 0 == checkFile(testPuppetFileWithPath, targetFilePerms, targetFileOwner, targetFileGroup), "DBus: file properties not expected")

    def test_run_string_bad_puppet_manifest(self):
        results = wrapper(qmf, 'string', 'bad puppet manifest', 0, 'puppet', testUtil.getRandomKey(5))
        self.assertTrue( results.get('status') == 'FAILED\n1', "QMF: result: " + str(results.get('status')) + " != FAILED\n1")
        self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "QMF: file properties not expected")

        if testUtil.haveDBus:
            results = wrapper(dbus, 'string', 'bad puppet manifest', 0, 'puppet', testUtil.getRandomKey(5))
            self.assertTrue(str(results) == 'FAILED\n1', "DBus: result: " + str(results) + " != FAILED\n1")
            self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "DBus: file properties not expected")

    def test_run_string_non_schema(self):
        self.assertRaises(QmfAgentException, wrapper, qmf, 'string', testUtil.getFileContents(testPuppetFileWithPath), 0, 'schema', testUtil.getRandomKey(5))
        self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "QMF: file properties not expected")

        if testUtil.haveDBus:
            self.assertRaises(DBusException, wrapper, dbus, 'string', testUtil.getFileContents(testPuppetFileWithPath), 0, 'schema', testUtil.getRandomKey(5))
            self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "DBus: file properties not expected")

    def test_run_string_empty_key(self):
        self.assertRaises(QmfAgentException, wrapper, qmf, 'string', testUtil.getFileContents(testPuppetFileWithPath), 0, 'puppet', '')
        self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "QMF: file properties not expected")

        if testUtil.haveDBus:
            self.assertRaises(DBusException, wrapper, dbus, 'string', testUtil.getFileContents(testPuppetFileWithPath), 0, 'puppet', '')
            self.assertTrue( 0 == checkFile(testPuppetFileWithPath, origFilePerms, origFileOwner, origFileGroup), "DBus: file properties not expected")

    # TEST - Augeas
    @attr('augeas')
    def test_run_string_good_augeas(self):
        results = wrapper(qmf, 'string', augeasFileContents, 0, 'augeas', testUtil.getRandomKey(5)).get('status')
        tokens = results.split('\n')
        self.assertEqual(tokens[0], 'OK', "QMF: result: %s != OK" % tokens[0])
        self.assertTrue(tokens[1].startswith("%s = " % augeasQuery), "QMF: wrong format of result (%s)" % tokens[1])

        if testUtil.haveDBus:
            results = wrapper(dbus, 'string', augeasFileContents, 0, 'augeas', testUtil.getRandomKey(5))
            tokens = results.split('\n')
            self.assertEqual(tokens[0], 'OK', "result: %s != OK" % tokens[0])
            self.assertTrue(tokens[1].startswith("%s = " % augeasQuery), "DBus: wrong format of result (%s)" % tokens[1])

    @attr('augeas')
    def test_run_string_bad_augeas(self):
        result = wrapper(qmf, 'string', 'bad augeas query', 0, 'augeas', testUtil.getRandomKey(5)).get('status')
        tokens = result.split('\n')
        self.assertEqual(tokens[0], 'FAILED', "QMF: result: %s != FAILED" % tokens[0])

        if testUtil.haveDBus:
            result = wrapper(dbus, 'string', 'bad augeas query', 0, 'augeas', testUtil.getRandomKey(5))
            tokens = result.split('\n')
            self.assertEqual(tokens[0], 'FAILED', "DBus: result: %s != FAILED" % tokens[0])

    # TEST - query()
    # ================================================================
    @attr('augeas')
    def test_query_good_augeas(self):
        result = qmf.query(augeasQuery, 0, 'augeas').get('data')
        self.assertNotEqual(result, 'unknown', "QMF result: %s == unknown" % result)

        if testUtil.haveDBus:
            result = dbus.query(augeasQuery, 0, 'augeas')
            self.assertNotEqual(result, 'unknown', "DBus result: %s == unknown" % result)

    @attr('augeas')
    def test_query_bad_augeas(self):
        result = qmf.query('bad augeas query', 0, 'augeas').get('data')
        self.assertEqual(result, 'unknown', "QMF result: %s != unknown" % result)

        if testUtil.haveDBus:
            result = dbus.query('bad augeas query', 0, 'augeas')
            self.assertEqual(result, 'unknown', "DBus result: %s != unknown" % result)

    # TEST - is_configured()
    # ================================================================
    def test_is_configured_known_key(self):
        key = testUtil.getRandomKey(5)
        wrapper(qmf, 'uri',testPuppetFileUrl, 0, 'puppet', key)
        results = qmf.is_configured(key)
        self.assertTrue( results.get('status') == 'OK', "QMF result: " + str(results.get('status')) + " != OK")

        if testUtil.haveDBus:
            key = testUtil.getRandomKey(5)
            wrapper(dbus, 'uri',testPuppetFileUrl, 0, 'puppet', key)
            results = dbus.is_configured(key)
            self.assertTrue(str(results) == 'OK', "DBus result: " + str(results) + " != OK")

    def test_is_configured_unknown_key(self):
        results = qmf.is_configured(testUtil.getRandomKey(5))
        self.assertTrue( results.get('status') == 'unknown', "QMF result: " + str(results.get('status')) + " != unknown")

        if testUtil.haveDBus:
            results = dbus.is_configured(testUtil.getRandomKey(5))
            self.assertTrue( str(results) == 'unknown', "DBus result: " + str(results) + " != unknown")

    def test_is_configured_failed_key(self):
        key = testUtil.getRandomKey(5)
        wrapper(qmf, 'string', "bad puppet manifest", 0, 'puppet', key)
        tokens = qmf.is_configured(key).get('status').split('\n')
        self.assertTrue(tokens[0] == 'FAILED', "QMF result: %s != FAILED" % tokens[0])

        if testUtil.haveDBus:
            key = testUtil.getRandomKey(5)
            wrapper(dbus, 'string', "bad puppet manifest", 0, 'puppet', key)
            tokens = dbus.is_configured(key).split('\n')
            self.assertTrue(tokens[0] == 'FAILED', "DBus result: %s != FAILED" % tokens[0])
