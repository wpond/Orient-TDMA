import serial
import struct
import time
import sys

#PACKETS = 100000
PACKETS = 10000
#TESTS = 20
TESTS = 1

s = serial.Serial(port="COM5",timeout=1)
#txPeriods = [x*100 for x in range(1,9)]
txPeriods = [300]
#txPeriods = [100,200,300,400,500,600,700,800,900]

f = open("speedtest.csv","w")
f.write("tx period, test number, packets, hits, misses, time (s), speed (kbps)\n")

for txP in txPeriods:
	for testNum in xrange(TESTS):
		
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
						1,
						0x02,
						0,
						False,
						102,
						1,
						10,
						100,
						300,
						50,
						True,
						0,
						0, # improvements enabled
						0)
		bsTdma = struct.pack("=BBBBBBBIIIBxxxxxxxxxxxx",
						0,
						0x02,
						0,
						True,
						102,
						1,
						10,
						100,
						txP,
						50,
						True)

		s.write(nodeTdma)
		time.sleep(0.5)
		s.write(bsTdma)

		dataOn = struct.pack("=BBBB28x",1,0x08,1,0)

		time.sleep(0.5)
		s.write(dataOn)

		while True:
			try:
				data = s.read(32)
				(type,) = struct.unpack("xB30x",data)
				if (type == 0x06):
					(frame,seg,flags) = struct.unpack("xxxBBxB25x",data)
					break
			except KeyboardInterrupt:
				break
			except:
				print sys.exc_info()[0]
				pass

		if flags & 0x01:
			seg = 0
			frame = (frame + 1) % 256
		else:
			seg += 1

		count = 0
		hits = 0
		misses = 0
		byteCount = 0
		t1 = time.time()
		for i in xrange(PACKETS):
			data = s.read(32)
			(type,nframe,nseg,bytes,nflags) = struct.unpack("xBxBBBB25x",data)
			if type == 0x06:
				if nframe == frame and nseg == seg:
					hits += 1
					byteCount += bytes
					frame = nframe
					seg = nseg
					flags = nflags
					
					if flags & 0x01:
						seg = 0
						frame = (frame + 1) % 256
					else:
						seg += 1
				else:
					misses += 1
			count += 1
		t2 = time.time()

		tp = t2 - t1
		kbps = (byteCount * 8) / (tp)
		
		print "tx period: %s [%s]" % (txP,testNum)
		print "total packets: %s" % count
		print "hits: %s" % hits
		print "misses: %s" % misses
		print "time taken: %s" % tp
		print "speed: %s kbps" % kbps
		
		#f.write("tx period, test number, packets, hits, misses, time (s), speed (kbps)\n")
		f.write("%s, %s, %s, %s, %s, %s, %s\n" % (txP,testNum,count,hits,misses,tp,kbps))
		
		dataOff = struct.pack("=BBBB28x",1,0x08,2,0)

		s.write(dataOff)

		nodeTdma = struct.pack("=BBB29x",
						1,
						0x03,
						False)
		bsTdma = struct.pack("=BBB29x",
						0,
						0x03,
						False)

		s.write(nodeTdma)
		time.sleep(0.5)
		s.write(bsTdma)
		
		time.sleep(1)

f.close()