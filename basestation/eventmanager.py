import struct

class EventManager:
	
	def __init__(self,parent):
		self.parent = parent
	
	def handle(self,packet):
		# get list of events
		events = packet[2:]
		for i in xrange(30):
			(e,) = struct.unpack("B",events[:1])
			
			if e == 0:
				break
			
			if e == 0xFF:
				self.parent.transport.sendAcks()
			
			events = events[1:]
	