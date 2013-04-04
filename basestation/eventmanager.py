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
			
			#print "EVENT RECVD [%d]" % e
			
			if e == 0xFE:
				self.parent.transport.ackSent()
			
			if e == 0xFF:
				#self.parent.transport.sendAcks()
				pass
			
			events = events[1:]
	