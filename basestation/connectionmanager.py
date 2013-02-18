import struct
import time
import orientserial

# time to wait to response in seconds
TIMEOUT = 0.025

class ConnectionManager:
	
	EVENTS = {
		"DATA_START": 1,
		"DATA_STOP": 2
	}
	
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
			
			for i in xrange(10):
				self.orient.send(configPacket)
				time.sleep(0.5)
				self.orient.sendHello()
				time.sleep(0.5)
				if not id in self.orient.connectedNodes:
					break
			
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
		
		return True
	
	def disableCaptureMode(self):
		packet = struct.pack("BBBBxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
			self.orient.BROADCAST_ID,
			self.orient.PACKET_TYPES["TDMA_ENABLE"],
			self.nextSequenceNumber(),
			False)
		
		for i in xrange(10):
			self.orient.send(packet)
			time.sleep(2)
			self.orient.sendHello() # get node list
			time.sleep(2)
			if not self.orient.connectedNodes:
				break
		
		# finally move basestation to aloha mode
		packet = struct.pack("BBBBxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
			self.orient.BASESTATION_ID,
			self.orient.PACKET_TYPES["TDMA_ENABLE"],
			self.nextSequenceNumber(),
			False)
		self.orient.send(packet)
		
		return True
	
	def sendEvent(self,events,id):
		headers = struct.pack("BB",
			id,
			self.orient.PACKET_TYPES["EVENT"])
		
		remainder = 30
		packet = headers
		for e in events:
			packet += struct.pack("B",
				e)
			remainder -= 1
			if remainder == 0:
				self.orient.send(packet)
				packet = headers
				remainder = 30
		if remainder < 30:
			packet += struct.pack("B" + str(remainder-1) + "x",
				0) # null terminating
			self.orient.send(packet)
		
		return True