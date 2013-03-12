import socket
import struct
import sys

UDP_IP = "0.0.0.0"
UDP_PORT = 9899

output = open("debuglog.txt","w")
c = 0

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(1)
sock.bind((UDP_IP,UDP_PORT))

try:
	s = ""
	while True:
		try:
			data,addr = sock.recvfrom(32)
		except socket.timeout:
			continue
		(len,end) = struct.unpack("xxB?28x",data)
		(m,) = struct.unpack(str(len) + "s",data[4:4+len])
		s += m
		if end:
			output.write(s)
			c += 1
			if c % 50 == 0:
				output.flush()
				c = 1
			if s[-1] == '\n':
				s = s[:-1]
			print s
			s = ""
			
except KeyboardInterrupt:
	pass
finally:
	sock.close()
	output.close()
