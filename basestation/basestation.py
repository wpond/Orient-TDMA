from orientserial import OrientSerial
import serial
import sys
import time
import struct
import connectionmanager
import shlex
import re

class Basestation:
	
	lsTimeout = 1
	
	def getInputIds(self,cmd,s):
		n = s[len(cmd + " "):]
		l = n.split(" ")
		return map(lambda x: int(x.strip(),0),l)
	
	def run(self):
		self.openOrientConnection("COM12")
		
		config = {
			"channel": 102,
			"slotCount": 10,
			"guardPeriod": 100,
			"transmitPeriod": 900,
			"protectionPeriod": 30,
		}
		
		try:
			
			print
			print "Orient TDMA Basestation Terminal v0.3a"
			
			alias = {
				"start": "ls; event 1 1; tdma 1;",
				"stop": "aloha; event 1 2; event 1 3; ack;",
				"data on": "event 1 1;",
				"data off": "event 1 2;",
				"clear": "event 1 3; ack;",
				"test req": "ls; request 1; ack",
			}
			
			running = True
			
			while running:
				
				print
				raw = raw_input("Enter command (? to list):\n>")
				
				aliasd = False
				if raw in alias:
					raw = alias[raw]
					aliasd = True
				
				cmds = re.findall(r'''((?:[^;"']|"[^"]*"|'[^']*')+)''',raw)
				for input in cmds:
					input = input.strip()
					if aliasd:
						print " %s" % input
					
					if input == "?":
						print
						print "================"
						print "= Command List ="
						print "================"
						print " quit"
						print
						print " ?"
						print "help"
						print
						print " ls"
						print "list all nodes available"
						print
						print " tdma 1 2...n"
						print "move nodes to TDMA mode"
						print
						print " aloha"
						print "move all nodes to aloha mode"
						print
						print " event id 1 2...n"
						print "send events with ids 1,2..n to id node"
						print
						print " ack"
						print "issue all outstanding acknowledgments"
						print 
						print " request 1 2...n"
						print "request a slot for node id 1"
						print
						print "	alias alias \"cmd\""
						print "aliases alias to cmd"
						print 
					elif input == "ls":
						self.orient.sendHello()
						l = set()
						for i in xrange(10):
							time.sleep(self.lsTimeout)
							if not self.orient.connectedNodes - l:
								break
							l |= self.orient.connectedNodes
						print ', '.join(map(str, l))
						
					elif input.startswith("tdma"):
						try:
							l = self.getInputIds("tdma",input)
						except:
							print "check formatting"
							print "e.g. tdma 1 2 3"
						if not self.orient.connectionmanager.enableCaptureMode(l,config):
							print "unable to complete tdma command"
						self.lsTimeout = 2
					elif input == "aloha":
						if not self.orient.connectionmanager.disableCaptureMode():
							print "unable to complete aloha command"
						self.lsTimeout = 1
					elif input.startswith("event"):
						l = self.getInputIds("event",input)
						if len(l) < 2:
							print "check formatting"
							print "e.g. event 1 2 sends node 1 event 2"
						id = l.pop(0)
						if not self.orient.connectionmanager.sendEvent(l,id):
							print "unable to send event"
					elif input == "ack":
						self.orient.transport.clearAcks()
						self.orient.transport.sendAcks()
					elif input.startswith("request"):
						l = self.getInputIds("request",input)
						for i in l:
							self.orient.alloc.request(i)
					elif input.startswith("alias"):
						try:
							(_,a,cmd) = shlex.split(input)
							alias[a] = cmd
						except:
							print "unable to alias, check formatting"
					elif input == "quit":
						running = False
						break
					elif not input == "":
						print "Unknown command"
				
		except KeyboardInterrupt:
			pass
		finally:
			
			self.closeOrientConnection()
	
	def openOrientConnection(self,com):
		try:
			self.orient = OrientSerial(com)
		except serial.SerialException:
			print "Unable to open serial port (" + com + ")"
			sys.exit(1)
	
	def closeOrientConnection(self):
		self.orient.stop()
	
if __name__ == "__main__":
	b = Basestation()
	b.run()
