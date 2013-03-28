import serial
import struct
import time
import sys

PACKETS = 10000
NODE_COUNT = 1

s = serial.Serial(port="COM5",timeout=1)

f = open("packetloss.csv","w")
f.write("nid, packet loss, test number, improvements, packets, hits, misses, time (s), data generation rate (kbps), speed (kbps)")

#for improvements in [False,True]:
	#for loss in [0,2,3,4,5,6,7,8,9,10,15,20,25,30]:

#for dataRate in [5,6,7,8,9,10,11,12,13]:
#for dataRate in [15,30,45]:

for dataRate in [0]:
	for testNum in xrange(1):
		
		improvements = False
		loss = 0
		
		for nid in range(1,NODE_COUNT+1):
			'''
			nodeTdma = struct.pack("=BBBBBBBIIIBxxxxxxxxxxxx",
							1,
							0x02,
							0,
							False,
							102,
							1,
							10,
							100,
							txP,
							50,
							True)
			'''
			nodeTdma = struct.pack("=BBBBBBBIIIBBBHxxxxxxxx",
							nid,
							0x02,
							0,
							False,
							102,
							nid,
							10,
							100,
							300,
							50,
							True,
							0, # packet loss
							0, # improvements enabled
							0) # datarate
			
			s.write(nodeTdma)
			time.sleep(0.5)
		
		bsTdma = struct.pack("=BBBBBBBIIIBBBHxxxxxxxx",
						0,
						0x02,
						0,
						True,
						102,
						1,
						10,
						100,
						900,
						50,
						True,
						0,
						0,
						0)

		s.write(bsTdma)

		dataOn = struct.pack("=BBBB28x",0xFF,0x08,1,0)

		time.sleep(0.5)
		s.write(dataOn)
				
		nodeStates = {}
		
		while len(nodeStates) < NODE_COUNT:
			try:
				data = s.read(32)
				(type,) = struct.unpack("xB30x",data)
				if (type == 0x06):
					(nid,frame,seg,flags) = struct.unpack("xxBBBxB25x",data)
					if flags & 0x01:
						seg = 0
						frame = (frame + 1) % 256
					else:
						seg += 1
					nodeStates[nid] = { 
						"frame" : frame, 
						"seg" : seg, 
						"byteCount" : 0,
						"hits" : 0,
						"misses": 0
					}
			except KeyboardInterrupt:
				print "state: waiting for data from all nodes"
				sys.exit(0)
			except:
				print sys.exc_info()[0]
				print "recvd: %s bytes" % len(data)
				pass
		
		
		print "test: %s" % testNum
		print "loss: %s" % loss
		if improvements:
			print "Improvements: enabled"
		else:
			print "Improvements: disabled"
		print "data generation rate %s kbps" % ((dataRate * 20 * 25 * 8)/1024.0)
		
		count = 0
		step = PACKETS * NODE_COUNT / 100
		sys.stdout.write(" |%s| 0%%" % (" " * 50))
		
		t1 = time.time()
		for i in xrange(PACKETS * NODE_COUNT):
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
				else:
					nodeStates[nid]["misses"] += 1
				count += 1
				if count % step == 0:
					percent = 100.0 * float(count) / float(PACKETS * NODE_COUNT)
					i = int(percent/2)
					sys.stdout.write("\r |%s%s|\t%i %%" % (("=" * i), (" " * (50-i)), int(percent)))
		t2 = time.time()
		sys.stdout.write("\r%s\r" % (" " * 65))
		tp = t2 - t1
		
		print "total packets: %s" % count
		print "time taken: %s" % tp
		for nid in nodeStates.iterkeys():
			nodeStates[nid]["bps"] = (nodeStates[nid]["byteCount"] * 8) / (tp)
			print "node: %s" % nid
			print "\thits: %s" % nodeStates[nid]["hits"]
			print "\tmisses: %s" % nodeStates[nid]["misses"]
			print "\tbytes: %s" % nodeStates[nid]["byteCount"]
			print "\tspeed: %s kbps" % (nodeStates[nid]["bps"] / 1024.0)
			print
			
			#f.write("tx period, test number, packets, hits, misses, time (s), speed (kbps)\n")
			f.write("%s, %s, %s, %s, %s, %s, %s, %s, %s, %s\n" % (nid,loss,testNum,improvements,nodeStates[nid]["hits"] + nodeStates[nid]["misses"],nodeStates[nid]["hits"],nodeStates[nid]["misses"],tp,((dataRate * 20 * 25 * 8)/1024.0),(nodeStates[nid]["bps"] / 1024)))
		
		dataOff = struct.pack("=BBBB28x",0xFF,0x08,2,0)

		s.write(dataOff)
		time.sleep(0.5)
		
		for nid in range(1,NODE_COUNT+1):
			nodeTdma = struct.pack("=BBB29x",
							nid,
							0x03,
							False)
			s.write(nodeTdma)
			time.sleep(0.5)
		
		bsTdma = struct.pack("=BBB29x",
						0,
						0x03,
						False)

		s.write(bsTdma)
		
		time.sleep(1)

f.close()