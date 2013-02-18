import serial
import Queue
from struct import *
import threading
import struct
import connectionmanager
import debugger
import eventmanager
import transport
import sys

class OrientSerial:
	
	BROADCAST_ID = 0xFF
	BASESTATION_ID = 0x00

	PACKET_TYPES = {
		"TDMA_TIMING": 0x00,
		"HELLO": 0x01,
		"TDMA_CONFIG": 0x02,
		"TDMA_ENABLE": 0x03,
		"TDMA_SLOT": 0x04,
		"TDMA_ACK": 0x05,
		"TRANSPORT_DATA": 0x06,
		"TRANSPORT_ACK": 0x07,
		"EVENT": 0x08,
		"BASESTATION_DEBUG": 0xFF,
	}
	
	connectedNodes = set()
	
	class OrientSender(threading.Thread):
		
		def __init__(self,conn):
			threading.Thread.__init__(self)
			self.running = True
			self.queue = Queue.Queue()
			self.conn = conn
		
		def run(self):
			while self.running:
				try:
					packet = self.queue.get(True,1)
				except Queue.Empty:
					continue
				self.conn.write(packet)
		
		def send(self,packet):
			self.queue.put(packet)
		
		def stop(self):
			self.running = False
	
	class OrientReceiver(threading.Thread):
		
		def __init__(self,conn):
			threading.Thread.__init__(self)
			self.running = True
			self.queue = Queue.Queue()
			self.conn = conn
		
		def run(self):
			packet = ""
			while self.running:
				try:
					c = self.conn.read(1)
					if c == "":
						continue
					packet += c
					if len(packet) >= 32:
						self.queue.put(packet)
						(addr,type) = struct.unpack("BBxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",packet)
						packet = ""
				except:
					continue
		
		def recv(self):
			try:
				packet = self.queue.get(False)
			except Queue.Empty:
				packet = None
			return packet
		
		def stop(self):
			self.running = False
	
	class PacketHandler(threading.Thread):
	
		def __init__(self,queue,parent):
			threading.Thread.__init__(self)
			self.queue = queue
			self.parent = parent
			self.running = True
		
		def run(self):
			while (self.running):
				try:
					packet = self.queue.get(False)
				except Queue.Empty:
					continue
				(addr,type) = struct.unpack("BBxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",packet)
				if type == self.parent.PACKET_TYPES["HELLO"]:
					(id,) = struct.unpack("xxBxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",packet)
					if not id in self.parent.connectedNodes:
						self.parent.connectedNodes.add(id)
				elif type == self.parent.PACKET_TYPES["TDMA_ACK"]:
					self.parent.connectionmanager.ackReceived(packet)
				elif type == self.parent.PACKET_TYPES["EVENT"]:
					self.parent.eventmanager.handle(packet)
				elif type == self.parent.PACKET_TYPES["TRANSPORT_DATA"]:
					self.parent.transport.recv(packet)
				elif type == self.parent.PACKET_TYPES["BASESTATION_DEBUG"]:
					self.parent.debugger.send(packet)
		
		def stop(self):
			self.running = False
		
	def __init__(self,port):
		self.port = port
		self.serial = serial.Serial(port=port,timeout=1)
		self.sender = OrientSerial.OrientSender(self.serial)
		self.recvr = OrientSerial.OrientReceiver(self.serial)
		self.connectionmanager = connectionmanager.ConnectionManager(self)
		self.debugger = debugger.Debugger()
		self.handler = OrientSerial.PacketHandler(self.recvr.queue,self)
		self.eventmanager = eventmanager.EventManager(self)
		self.transport = transport.Transport(self)
		
		self.sender.start()
		self.recvr.start()
		self.handler.start()
	
	def send(self,packet):
		if not len(packet) == 32:
			print "INVALID PACKET SIZE [%d]" % len(packet)
			return
		self.sender.send(packet)
	
	def stop(self):
		self.sender.stop()
		self.recvr.stop()
		self.handler.stop()
		
		self.sender.join()
		self.recvr.join()
		self.handler.join()
		
		self.serial.close()
	
	def sendHello(self,id=BROADCAST_ID):
		if id == self.BROADCAST_ID:
			self.connectedNodes = set()
		elif id in self.connectedNodes:
			self.connectedNodes.remove(id)
		packet = struct.pack("BBBxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
			id,
			self.PACKET_TYPES["HELLO"],
			0xFF)
		self.send(packet)
