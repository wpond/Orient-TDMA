import serial
import struct
import time
import sys

PACKETS = 30000

s = serial.Serial(port="COM4",timeout=1)

f = open("packetloss.csv","w")
f.write("packet loss, test number, improvements, packets, hits, misses, time (s), speed (kbps)\n")

for improvements in [False,True]:
	for loss in [0,2,3,4,5,6,7,8,9,10,15,20,25,30]:
		for testNum in xrange(5):
			
			nodeTdma = struct.pack("=BBBBBBBIIIBBBxxxxxxxxxx",
							1,
							0x02,
							0,
							False,
							102,
							1,
							10,
							100,
							900,
							50,
							True,
							loss,
							improvements) # improvements enabled
			bsTdma = struct.pack("=BBBBBBBIIIBBBxxxxxxxxxx",
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
							improvements)

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
					print "recvd: %s" % len(data)
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
			
			print "loss: %s [%s]" % (loss,testNum)
			if improvements:
				print "Improvements: enabled"
			else:
				print "Improvements: disabled"
			print "total packets: %s" % count
			print "hits: %s" % hits
			print "misses: %s" % misses
			print "time taken: %s" % tp
			print "speed: %s kbps" % kbps
			print
			
			#f.write("tx period, test number, packets, hits, misses, time (s), speed (kbps)\n")
			f.write("%s, %s, %s, %s, %s, %s, %s, %s\n" % (loss,testNum,improvements,count,hits,misses,tp,kbps))
			
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