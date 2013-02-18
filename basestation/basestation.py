from orientserial import OrientSerial
import serial
import sys
import time
import struct
import connectionmanager

class Basestation:
	
	lsTimeout = 1
	
	def getInputIds(self,cmd,s):
		n = s[len(cmd + " "):]
		l = n.split(" ")
		return map(lambda x: int(x.strip()),l)
	
	def run(self):
		self.openOrientConnection("COM12")
		
		config = {
			"channel": 102,
			"slotCount": 40,
			"guardPeriod": 234,
			"transmitPeriod": 937,
			"protectionPeriod": 117,
		}
		
		try:
			
			print
			print "Orient TDMA Basestation Terminal v0.3a"
			
			while True:
				print
				input = raw_input("Enter command (? to list):\n>")
				
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
					print " tdma 1 2 ... n"
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
					self.orient.transport.sendAcks()
				elif input == "quit":
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
