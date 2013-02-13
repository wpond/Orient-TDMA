import socket
import struct

UDP_IP = "0.0.0.0"
UDP_PORT = 9899

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
			if s[-1] == '\n':
				s = s[:-1]
			print s
			s = ""
except KeyboardInterrupt:
	pass
finally:
	sock.close()
