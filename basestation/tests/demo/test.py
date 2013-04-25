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
			data,addr = sock.recvfrom(64)
		except socket.timeout:
			continue
		print data
			
except KeyboardInterrupt:
	pass
finally:
	sock.close()
	output.close()
