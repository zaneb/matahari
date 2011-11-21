#!/usr/bin/env python

"""
  matahariTest.py - Copyright (c) 2011 Red Hat, Inc.
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

import time
import commands as cmd
import cqpid
from qmf2 import ConsoleSession
import sys
import os
from stat import *
from pwd import getpwuid
from grp import getgrgid
import random
import string
import subprocess
import logging

haveDBus = True
try:
    import dbus
except ImportError:
    sys.stderr.write("Unable to find dbus-python module, DBus tests will be disabled\n")
    haveDBus = False


class TestsSetup(object):
    def __init__(self, qmf_binary, agentKeyword, agentClass, dbus_binary=None, dbus_settings=[]):
        self.broker = MatahariBroker()
        self.broker.start()
        time.sleep(3)
        self.qmf_agent = MatahariQMFAgent(qmf_binary)
        self.qmf_agent.start()

        if dbus_binary is None:
            global haveDBus
            haveDBus = False
        if haveDBus:
            self.dbus_agent = MatahariDBusAgent(dbus_binary)
            self.dbus_agent.start()
        else:
            self.dbus_agent = None

        time.sleep(3)

        self._connectToBroker('localhost', '49001')
        if haveDBus:
            self.dbus = MatahariDBusObject(*dbus_settings)
        else:
            self.dbus = None

        self.agentKeyword = agentKeyword
        self.agentClass = agentClass
        self.qmf = self._findAgent(cmd.getoutput('hostname'))
        self.reQuery()

    def tearDown(self):
        self._disconnect()
        if self.dbus_agent is not None:
            self.dbus_agent.stop()
        self.qmf_agent.stop()
        self.broker.stop()

    def _disconnect(self):
        self.session.close()
        self.connection.close()

    def reQuery(self):
        self.qmf.update()
        self.qmf.props = self.qmf.getProperties()
        if haveDBus:
            self.dbus.reQuery()

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


def restartService(serviceName):
    cmd.getoutput("service " + serviceName + " restart")

# DBus
# ========================================================
class MatahariDBusObject(object):
    def __init__(self, bus_name, object_path, interface):
        self.interface = interface
        bus = dbus.SystemBus()
        self.obj = bus.get_object(bus_name, object_path)
        self.iface = dbus.Interface(self.obj, dbus_interface=interface)

        self.reQuery()

    def reQuery(self):
        # Properties
        self.props = self.obj.GetAll(self.interface, dbus_interface='org.freedesktop.DBus.Properties')

    def get(self, prop):
        return self.props[prop]

    def __getattr__(self, attr):
        return self.iface.get_dbus_method(attr)

# Files
# ========================================================
def getFilePermissionMask(file):
    return oct(os.stat(file)[ST_MODE])[-3:]

def getFileOwner(file):
    return getpwuid(os.stat(file).st_uid).pw_name

def getFileGroup(file):
    return getgrgid(os.stat(file).st_gid).gr_name

def getFileContents(file):
    f = open(file, 'r')
    return f.read()

def setFileContents(file, content):
    file = open(file, 'w')
    file.write(content)
    file.close

# Other
# =======================================================
def getDelta(int1, int2):
     delta = 0
     if int1 > int2:
          delta = int1 - int2
     elif int2 > int1:
          delta = int2 - int1
     # else it is even which is a test success
     else:
          delta = 0
     return delta

def checkTwoValuesInMargin(val1, val2, fudgeFactor, description):
    delta = getDelta(val1,val2)
    margin = (val1+100) * fudgeFactor
    #percent_str = str(fudgeFactor * 100) + "%"
    #delta > margin: error(description + " value gt "+ percent_str + " off (va1:"+str(val1)+" val2:" + str(val2) + ")")
    return delta < margin

def getRandomKey(length):
    return ''.join(random.choice(string.ascii_uppercase + string.digits) for x in range(length))


class MatahariAgent(object):
    def __init__(self, agent_name):
        self.agent_name = agent_name
        self.agent = None

    def start(self):
        raise NotImplementedError("Method start() is not implemented")

    def stop(self):
        sys.stderr.write("Stopping %s ...\n" % self.agent_name)
        self.agent.terminate()
        output, output_err = self.agent.communicate()
        sys.stderr.write("********** Stopped %s, stdout follows: *************\n" %
                     self.agent_name)
        sys.stderr.write(output + "\n")
        sys.stderr.write("********************************************************\n")
        sys.stderr.write("********** Stopped %s, stderr follows: *************\n" %
                     self.agent_name)
        sys.stderr.write(output_err + "\n")
        sys.stderr.write("********************************************************\n")

class MatahariBroker(MatahariAgent):
    def __init__(self):
        MatahariAgent.__init__(self, "broker")

    def start(self):
        sys.stderr.write("Starting broker ...\n")
        self.agent = subprocess.Popen(["qpidd", "--auth", "no", "--port", "49001"],
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE)

class MatahariQMFAgent(MatahariAgent):
    def start(self):
        sys.stderr.write("Starting %s ...\n" % self.agent_name)
        self.agent = subprocess.Popen([self.agent_name, "--reconnect", "yes",
                                       "--broker", "127.0.0.1", "--port", "49001", "-v"],
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE)

class MatahariDBusAgent(MatahariAgent):
    def start(self):
        if self.agent_name is None:
            return
        sys.stderr.write("Starting %s ...\n" % self.agent_name)
        self.agent = subprocess.Popen(["%s" % self.agent_name, "-vvv"],
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE)
