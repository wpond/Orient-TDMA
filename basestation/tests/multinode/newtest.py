import struct, time, sys, serial, os

HEADERS = ["test","node id","transmit period","packet loss","optimisations","data rate","total","hits","misses","throughput"]

TEST_COUNT = 1
TEST_NODES = [1,2,3,4]
TEST_TXPS = [300]
TEST_LOSS = [0]
TEST_OPTIMISATIONS = [True]
TEST_DATE_RATE = [20]
TEST_PACKETS = 10000
TEST_SLOTS = [0]
TEST_SLOT_LEASES = [5]

def runTest(s,f,id,txp,loss,optimisations,data_rate,slots,lease):
	# enable nodes
	for nid in TEST_NODES:
		nodeTdma = struct.pack("=BBBBBBBIIIBBBHxxxxxxxx",
							nid,
							0x02,
							0,
							False,
							102,
							nid,
							10,
							100,
							txp,
							50,
							True,
							loss, # packet loss
							optimisations, # improvements enabled
							data_rate) # datarate
		s.write(nodeTdma)
		time.sleep(0.2)
	
	bsTdma = struct.pack("=BBBBBBBIIIBBBxxxxxxxxxx",
						0,
						0x02,
						0,
						True,
						102,
						1,
						10,
						100,
						txp,
						50,
						True,
						slots,
						lease)
	s.write(bsTdma)
	
	sys.stdout.write("Waiting to stabilise")
	
	time.sleep(2)
	s.flushInput()
	
	sys.stdout.write("\r")
	sys.stdout.write(" |%s| 0%%" % (" " * 50))
	
	nodeStates = {}
	step = TEST_PACKETS * len(TEST_NODES) / 100
	count = 0
	t1 = time.time()
	for i in xrange(TEST_PACKETS * len(TEST_NODES)):
		data = s.read(32)
		(type,nid,nframe,nseg,nbytes,nflags) = struct.unpack("xBBBBBB25x",data)
		if type == 0x06:
			if nid in nodeStates and nframe == nodeStates[nid]["frame"] and nseg == nodeStates[nid]["seg"]:
				nodeStates[nid]["hits"] += 1
				nodeStates[nid]["byteCount"] += nbytes
				if nflags & 0x01:
					nodeStates[nid]["seg"] = 0
					nodeStates[nid]["frame"] = (nframe + 1) % 256
				else:
					nodeStates[nid]["seg"] += 1
			elif nid in nodeStates:
				nodeStates[nid]["misses"] += 1
			else:
				nodeStates[nid] = { 
					"frame" : nframe, 
					"seg" : nseg, 
					"byteCount" : nbytes,
					"hits" : 1,
					"misses": 0
				}
				if nflags & 0x01:
					nodeStates[nid]["seg"] = 0
					nodeStates[nid]["frame"] = (nframe + 1) % 256
				else:
					nodeStates[nid]["seg"] += 1
			'''
			count += 1
			if count % step == 0:
				percent = 100.0 * float(count) / float(TEST_PACKETS * len(TEST_NODES))
				i = int(percent/2)
				sys.stdout.write("\r |%s%s|\t%i %%" % (("=" * i), (" " * (50-i)), int(percent)))
			'''
	t2 = time.time()
	tp = t2 - t1
	
	sys.stdout.write("\r%s\r" % (" " * 65))
	
	nodeTdma = struct.pack("=BBB29x",
							0xFF,
							0x03,
							False)
	s.write(nodeTdma)
	time.sleep(0.2)
	bsTdma = struct.pack("=BBB29x",
						0,
						0x03,
						False)
	s.write(bsTdma)
	time.sleep(1)
	
	print "test id: %s" % id
	print "unreserved slots: %s" % slots
	print "slot lease: %s" % lease
	print "packets received: %s" % count
	print "time taken: %s" % tp
	print "generation rate: %s" % ((data_rate * 20 * 25 * 8)/1024.0)
	print "transmit period: %s" % txp
	print "packet loss: %s" % loss
	print "optimisations: %s" % optimisations
	for nid in nodeStates.iterkeys():
		nodeStates[nid]["bps"] = (nodeStates[nid]["byteCount"] * 8) / (tp)
		print "node: %s" % nid
		print "\thits: %s" % nodeStates[nid]["hits"]
		print "\tmisses: %s" % nodeStates[nid]["misses"]
		print "\tbytes: %s" % nodeStates[nid]["byteCount"]
		print "\tspeed: %s kbps" % (nodeStates[nid]["bps"] / 1024.0)
	
		f.write("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" % (id,nid,txp,loss,optimisations,((data_rate * 20 * 25 * 8)/1024.0),nodeStates[nid]["hits"] + nodeStates[nid]["misses"],nodeStates[nid]["hits"],nodeStates[nid]["misses"],(nodeStates[nid]["bps"] / 1024)))
	
	print	
	
	
s = serial.Serial(port="COM5",timeout=1)

file_base = "output"
file_ext = ".csv"
i = 1
file = file_base + file_ext
while os.path.exists(file):
	file = file_base + str(i) + file_ext
	i += 1
f = open(file,"w")
f.write(','.join(HEADERS) + "\n")

for test in xrange(TEST_COUNT):
	for txp in TEST_TXPS:
		for loss in TEST_LOSS:
			for optimisations in TEST_OPTIMISATIONS:
				for data_rate in TEST_DATE_RATE:
					for slots in TEST_SLOTS:
						for lease in TEST_SLOT_LEASES:
							runTest(s,f,test,txp,loss,optimisations,data_rate,slots,lease)

print "RESULTS: %s" % file

s.close()
f.close()
