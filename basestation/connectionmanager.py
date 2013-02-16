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
	
	def enableCaptureMode(self,ids,config):
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
			
			while not id in self.orient.connectedNodes:
				self.orient.sendHello()
				time.sleep(TIMEOUT)
				self.orient.recvPacket()
			
			while id in self.orient.connectedNodes:
				self.orient.send(configPacket)
				time.sleep(TIMEOUT)
				self.orient.sendHello()
				time.sleep(TIMEOUT)
				self.orient.recvPacket()
			
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
		
		# send a load of messages to get all nodes working correctly?
		for i in range(3):
			for j in range(30):
				self.orient.sendHello()
			time.sleep(1)
			self.orient.recvPacket()
		
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
		
		i = 0
		while self.orient.connectedNodes:
			self.orient.send(packet)
			time.sleep(1)
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
