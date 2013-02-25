import threading
import time
import Queue
import struct

class BandwidthMonitor(threading.Thread):
	
	running = True
	
	def __init__(self,parent):
		threading.Thread.__init__(self)
		self.parent = parent
	
	def run(self):
		while self.running:
			time.sleep(1)
			data = self.parent.transport.recvPackets()
			for id,q in data.iteritems():
				c = 0
				try:
					while True:
						p = q.get(False)
						valid = True
						for i in xrange(len(p)):
							(b,) = struct.unpack("B",p[i])
							#print b
							if not b == i:
								valid = False
						if valid:
							c += len(p)
						else:
							print "Received corrupt packet"
							s = ""
							for i in xrange(len(p)):
								(b,) = struct.unpack("B",p[i])
								s += str(b) + " "
							print s
							print "==="
				except Queue.Empty:
					pass
				if c > 0:
					kbps = (c * 8) / 1024.0
					print "ID %s\t%s kbps" % (id,kbps)
	
	def stop(self):
		self.running = False
	