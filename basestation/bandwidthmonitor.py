import threading
import time
import Queue
import struct

class BandwidthMonitor(threading.Thread):
	
	running = True
	
	def __init__(self,parent):
		threading.Thread.__init__(self)
		self.parent = parent
		self.output = open('output.csv','w')
		self.output.write('Node 1, Node 2\n')
	
	def run(self):
		while self.running:
			time.sleep(1)
			data = self.parent.transport.recvPackets()
			rows = []
			for id in sorted(data.iterkeys()):
				q = data[id]
				c = 0
				try:
					while True:
						p = q.get(False)
						valid = True
						for i in xrange(len(p)):
							(b,) = struct.unpack("B",p[i])
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
					rows.append(str(c))
			if len(rows) == 2:
				self.output.write(','.join(rows) + '\n')
		self.output.close()
	
	def stop(self):
		self.running = False
	