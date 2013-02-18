import struct

FLAG_SEGMENT_END = 0x01

class Transport:
	
	acks = {}
	
	def __init__(self,parent):
		self.parent = parent
	
	def recv(self,packet):
		print struct.unpack("xxBBBBB25x",packet)
		(senderId,frameId,segmentId,segmentFill,flags) = struct.unpack("xxBBBBB25x",packet)
		valid = False
		EoF = False
		
		if senderId in self.acks:
			if self.acks[senderId]["flags"] & FLAG_SEGMENT_END:
				if (self.acks[senderId]["lastFrameId"] + 1) % 0xFF == frameId and segmentId == 0:
					self.acks[senderId]["lastFrameId"] = frameId
					self.acks[senderId]["lastSegmentId"] = segmentId
					self.acks[senderId]["flags"] = flags
					valid = True
					EoF = True
			elif self.acks[senderId]["lastFrameId"] == frameId and self.acks[senderId]["lastSegmentId"] + 1 == segmentId:
				self.acks[senderId]["lastSegmentId"] = segmentId
				self.acks[senderId]["flags"] = flags
				valid = True
		else:
			self.acks[senderId] = {
				"lastFrameId": frameId,
				"lastSegmentId": segmentId,
				"flags": flags,
			}
			valid = True
		
		if not valid:
			if self.acks[senderId]["flags"] & FLAG_SEGMENT_END:
				expectedFrame = (self.acks[senderId]["lastFrameId"] + 1) % 0xFF
				expectedSegment = 0
			else:
				expectedFrame = self.acks[senderId]["lastFrameId"]
				expectedSegment = self.acks[senderId]["lastSegmentId"] + 1
			print "Expected %d/%d but received %d/%d (seg/frame)" % (
				expectedSegment,
				expectedFrame,
				segmentId,
				frameId)
		
		# if EoF forward data (or print)
		if EoF:
			pass
	
	def sendAcks(self):
		for id in self.parent.connectedNodes:
			if not id in self.acks:
				packet = struct.pack("BBBB28x",id,self.parent.PACKET_TYPES["TRANSPORT_ACK"],0xFE,0xFE)
				self.parent.send(packet)
				print "ACK %d (seg/frame): %d/%d" % (id,0xFE,0xFE)
		for id,state in self.acks.items():
			packet = struct.pack("BBBB28x",id,self.parent.PACKET_TYPES["TRANSPORT_ACK"],state["lastFrameId"],state["lastSegmentId"])
			self.parent.send(packet)
			print "ACK %d (seg/frame): %d/%d" % (id,state["lastSegmentId"],state["lastFrameId"])