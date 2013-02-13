from orientserial import OrientSerial
import serial
import sys
import time
import struct
import connectionmanager

class Basestation:
	
	def run(self):
		self.openOrientConnection("COM12")
		
		tdmaconfig = {
			"channel": 102,
			"slotCount": 40,
			"guardPeriod": 234,
			"transmitPeriod": 937,
			"protectionPeriod": 117,
		}
		
		try:
			self.orient.connectionmanager.configureCapture([1],tdmaconfig)
			time.sleep(5)
			self.orient.recvPacket()
		except KeyboardInterrupt:
			pass
		finally:
			self.orient.connectionmanager.disableCaptureMode()
			time.sleep(1.5)
			self.orient.recvPacket()
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
