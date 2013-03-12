import struct

class Alloc:
	
	slots = {}
	seqNum = 0
	slotSpacing = 4
	slotSpacingOffsets = [0,2,1,3]
	currentSpacing = 0
	lease = 10
	waitingAllocations = {}
	
	def __init__(self,offset,count):
		for i in xrange(offset,count+offset):
			self.resetSlot(i)
		self.nextSlotId = offset - self.slotSpacing
		self.offset = offset
		self.count = count
	
	def nodesSlot(self,id):
		for sid,slot in self.slots.items():
			if slot["owner"] == id:
				return sid
		return -1
	
	def request(self,id):
		sid = self.nodesSlot(id)
		if sid == -1:
			self.alloc(self.nextAvailableSlot(),id)
			return True
		
		try:
			curSlot = sid + 1
			while self.slots[curSlot]["owner"] == id:
				curSlot += 1
			if self.slots[curSlot]["owner"] == -1:
				self.slots[curSlot]["ack"] = -2
				self.slots[sid]["ack"] = -1
				self.alloc(curSlot,id)
				return True
		except KeyError:
			pass
		
		try:
			curSlot = sid - 1
			while self.slots[curSlot]["owner"] == id:
				curSlot -= 1
			if self.slots[curSlot]["owner"] == -1:
				self.slots[curSlot]["ack"] = -1
				self.slots[sid]["ack"] = -2
				self.alloc(curSlot,id)
				return True
		except KeyError:
			pass
		
		return False
	
	def alloc(self,sid,id):
		self.slots[sid]["owner"] = id
		self.slots[sid]["lease"] = self.lease
		self.slots[sid]["ack"] = self.nextSeqNum()
	
	def createAlloc(self):
		length = 0
		owner = -2
		lease = 0
		stop = False
		seenIds = []
		
		for sid,slot in self.slots.items():
			if slot["lease"] > 0:
				slot["lease"] -= 1
				if slot["lease"] == 0:
					self.resetSlot(sid)
					stop = True
					try:
						self.slots[sid+1]["ack"] = -1
					except KeyError:
						pass
			
			if slot["owner"] in seenIds:
				self.resetSlot(sid)
				stop = True
				
			elif length == 0 and not slot["owner"] == -1 and not slot["ack"] == -2:
				length = 1
				owner = slot["owner"]
				lease = slot["lease"]
				
			elif slot["owner"] == owner and slot["lease"] > 0:
				lease = min(lease,slot["lease"])
				length += 1
				self.slots[sid]["ack"] = -1 # reset ack as we're only expecting ack for original slot
			
			else:
				stop = True
			
			if stop and length > 0:
				if slot["ack"] < 0:
					self.slots[sid-length]["ack"] = self.nextSeqNum()
				self.storeAlloc(sid-length,owner,length,lease,self.slots[sid-length]["ack"])
				seenIds.append(owner)
				length= 0
				owner = -2
				stop = False
			elif stop:
				stop = False
	
	def storeAlloc(self,sid,id,slotCount,lease,seq):
		# create and send packet
		print "Send allocation of %s for %s slots to node %s for %s seconds [ACK %d]" % (sid,slotCount,id,lease,seq)
		partialPacket = struct.pack("BBBBB",True,seq,sid,slotCount,lease)
		self.waitingAllocations[id] = partialPacket
	
	def getAllocs(self):
		self.createAlloc()
		return self.waitingAllocations
	
	def clearAllocs(self):
		self.waitingAllocations = {}
	
	def handleAck(self,packet):
		# set seqnum from packet
		(seqNum,) = struct.unpack("xxB29x",packet)
		for sid,slot in self.slots.items():
			if slot["ack"] == seqNum:
				print "ACK [%d] for slot %s" % (seqNum,slot)
				slot["ack"] = -2
	
	def resetSlot(self,sid):
		try:
			self.slots[sid]["owner"] = -1
			self.slots[sid]["lease"] = 0
			self.slots[sid]["ack"] = -1
		except KeyError:
			self.slots[sid] = {
				"owner": -1,
				"lease": 0,
				"ack": -2
			}
	
	def nextSeqNum(self):
		self.seqNum = (self.seqNum + 1) % 0xFF
		# if there is anything outstanding with this sequence number, reset it
		for sid,slot in self.slots.items():
			if slot["ack"] == self.seqNum:
				self.resetSlot(sid)
		return self.seqNum
	
	def nextAvailableSlot(self):
		for i in xrange(self.count):
			self.nextSlotId += self.slotSpacing
			if self.nextSlotId >= self.offset + self.count:
				self.nextSlotId = self.offset + self.nextSlotSpacing()
			if self.slots[self.nextSlotId]["owner"] == -1:
				return self.nextSlotId
		return -1
	
	def nextSlotSpacing(self):
		self.currentSpacing = (self.currentSpacing + 1) % len(self.slotSpacingOffsets)
		return self.slotSpacingOffsets[self.currentSpacing]
	