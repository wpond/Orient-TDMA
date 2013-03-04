import struct
import Queue

FLAG_SEGMENT_END = 0x01

class Transport:
	
	acks = {}
	outstandingAcks = 0
	packets = {}
	
	def __init__(self,parent):
		self.parent = parent
	
	def recv(self,packet):
		#print struct.unpack("xxBBBBB25x",packet)
		(senderId,frameId,segmentId,segmentFill,flags) = struct.unpack("xxBBBBB25x",packet)
		valid = False
		
		if senderId in self.acks:
			if self.acks[senderId]["flags"] & FLAG_SEGMENT_END:
				if (self.acks[senderId]["lastFrameId"] + 1) % 0xFF == frameId and segmentId == 0:
					self.acks[senderId]["lastFrameId"] = frameId
					self.acks[senderId]["lastSegmentId"] = segmentId
					self.acks[senderId]["flags"] = flags
					valid = True
			elif self.acks[senderId]["lastFrameId"] == frameId and self.acks[senderId]["lastSegmentId"] + 1 == segmentId:
				self.acks[senderId]["lastSegmentId"] = segmentId
				self.acks[senderId]["flags"] = flags
				valid = True
		else:
			self.acks[senderId] = {
				"lastFrameId": frameId,
				"lastSegmentId": segmentId,
				"flags": flags,
				"partialPacket": "",
			}
			valid = True
		
		# debug info
		if not valid:
			if self.acks[senderId]["flags"] & FLAG_SEGMENT_END:
				expectedFrame = (self.acks[senderId]["lastFrameId"] + 1) % 0xFF
				expectedSegment = 0
			else:
				expectedFrame = self.acks[senderId]["lastFrameId"]
				expectedSegment = self.acks[senderId]["lastSegmentId"] + 1
			'''
			print "Expected %d/%d but received %d/%d (seg/frame)" % (
				expectedSegment,
				expectedFrame,
				segmentId,
				frameId)
			'''
		
		if valid:
			self.acks[senderId]["partialPacket"] += packet[7:7+segmentFill]
			
			EoF = flags & FLAG_SEGMENT_END
			'''
			s = ""
			if EoF:
				print "FRAGMENT %d/%d EOF" % (segmentId,frameId)
			else:
				print "FRAGMENT %d/%d" % (segmentId,frameId)
			print "==="
			for i in xrange(segmentFill):
				(b,) = struct.unpack("B",packet[i+7])
				s += str(b) + " "
			print s
			print "==="
			'''
		
		# if EoF forward data (or print)
		if valid and EoF:
			if not senderId in self.packets:
				self.packets[senderId] = Queue.Queue()
			self.packets[senderId].put(self.acks[senderId]["partialPacket"])
			self.acks[senderId]["partialPacket"] = ""
	
	def recvPackets(self):
		return self.packets
	def clearPackets(self):
		self.packets = {}
	
	def ackSent(self):
		#print "ack sent"
		self.outstandingAcks -= 1
	
	def clearAcks(self):
		self.outstandingAcks = 0
	
	def sendAcks(self):
		if self.outstandingAcks == 0:
			slotAllocs = self.parent.alloc.getAllocs()
			self.parent.alloc.clearAllocs()
			
			for id in self.parent.connectedNodes:
				if not id in self.acks:
					packet = struct.pack("BBBB",id,self.parent.PACKET_TYPES["TRANSPORT_ACK"],0xFE,0xFE)
					if id in slotAllocs:
						packet += slotAllocs[id]
						del slotAllocs[id]
					else:
						packet += struct.pack("B",False)
					packet += struct.pack(str(32-len(packet)) + "x")
					
					self.parent.send(packet)
					self.outstandingAcks += 1
					print "ACK %d (seg/frame): %d/%d" % (id,0xFE,0xFE)
			
			for id,state in self.acks.items():
				packet = struct.pack("BBBB",id,self.parent.PACKET_TYPES["TRANSPORT_ACK"],state["lastFrameId"],state["lastSegmentId"])
				if id in slotAllocs:
					packet += slotAllocs[id]
					del slotAllocs[id]
				else:
					packet += struct.pack("B",False)
				packet += struct.pack(str(32-len(packet)) + "x")
				
				self.parent.send(packet)
				print "ACK %d (seg/frame): %d/%d" % (id,state["lastSegmentId"],state["lastFrameId"])
				self.outstandingAcks += 1
			
			if slotAllocs:
				print "Unable to send the following allocations"
				print slotAllocs
			