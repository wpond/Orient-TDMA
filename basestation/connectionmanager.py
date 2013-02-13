import struct
import time
import orientserial

# time to wait to response in seconds
TIMEOUT = 0.025

class ConnectionManager:
	
	sequenceNumber = 0
	lastAck = 0
	
	def __init__(self,orient):
		self.orient = orient
	
	def nextSequenceNumber(self):
		self.sequenceNumber = (self.sequenceNumber + 1) % 16
		return self.sequenceNumber
	
	def ackReceived(self,packet):
		(self.lastAck,) = struct.unpack("xxBxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",packet)
	
	def configureCapture(self,ids,config):
		if config["slotCount"] < len(ids):
			return False
		slot = 0
		for id in ids:
			slot += 1
			configPacket = struct.pack("=BBBBBBBIIIBxxxxxxxxxxxx",
				id,
				self.orient.PACKET_TYPES["TDMA_CONFIG"],
				self.nextSequenceNumber(),
				False,
				config["channel"],
				slot,
				config["slotCount"],
				config["guardPeriod"],
				config["transmitPeriod"],
				config["protectionPeriod"],
				True)
			self.orient.send(configPacket)
			time.sleep(1)
			self.orient.sendHello()
			time.sleep(1)
			if id in self.orient.connectedNodes:
				print "node didn't change"
		
		# finally move basestation to tdma mode
		configPacket = struct.pack("=BBBBBBBIIIBxxxxxxxxxxxx",
				self.orient.BASESTATION_ID,
				self.orient.PACKET_TYPES["TDMA_CONFIG"],
				self.nextSequenceNumber(),
				True,
				config["channel"],
				0,
				config["slotCount"],
				config["guardPeriod"],
				config["transmitPeriod"],
				config["protectionPeriod"],
				True)
		self.orient.send(configPacket)
		
		for j in range(5):
			for i in range(30):
				self.orient.sendHello()
			time.sleep(1.1)
		
		return True
		
	def enableCaptureMode(self,ids,config):
		if config["slotCount"] < len(ids):
			return False
		slot = 0
		for id in ids:
			slot += 1
			configPacket = struct.pack("=BBBBBBBIIIBxxxxxxxxxxx",
				id,
				self.orient.PACKET_TYPES["TDMA_CONFIG"],
				self.nextSequenceNumber(),
				False,
				config["channel"],
				slot,
				config["slotCount"],
				config["guardPeriod"],
				config["transmitPeriod"],
				config["protectionPeriod"],
				True)
			self.orient.send(configPacket)
			time.sleep(TIMEOUT)
			self.orient.sendHello()
			time.sleep(TIMEOUT)
			if id in self.orient.connectedNodes:
				print "node didn't change"
		
		# finally move basestation to tdma mode
		configPacket = struct.pack("=BBBBBBBIIIBxxxxxxxxxxx",
				self.orient.BASESTATION_ID,
				self.orient.PACKET_TYPES["TDMA_CONFIG"],
				self.nextSequenceNumber(),
				True,
				config["channel"],
				0,
				config["slotCount"],
				config["guardPeriod"],
				config["transmitPeriod"],
				config["protectionPeriod"],
				True)
		self.orient.send(configPacket)
		
		return True
	
	def disableCaptureMode(self):
		packet = struct.pack("BBBBxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
			self.orient.BROADCAST_ID,
			self.orient.PACKET_TYPES["TDMA_ENABLE"],
			self.nextSequenceNumber(),
			False)
		
		self.orient.sendHello() # get node list
		time.sleep(1)
		self.orient.recvPacket()
		print self.orient.connectedNodes
		
		while self.orient.connectedNodes:
			self.orient.sendHello() # get node list
			time.sleep(1)
			self.orient.recvPacket()
			print self.orient.connectedNodes
		
		# finally move basestation to aloha mode
		packet = struct.pack("BBBBxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
			self.orient.BASESTATION_ID,
			self.orient.PACKET_TYPES["TDMA_ENABLE"],
			self.nextSequenceNumber(),
			False)
		self.orient.send(packet)
