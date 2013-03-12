import socket, struct

UDP_IP = "127.0.0.1"
UDP_PORT = 9899

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

msg = "this is a longer test hello hello hello helloEND"
while len(msg) > 0:
	if len(msg) > 28:
		l = 28
		end = False
	else:
		l = len(msg)
		end = True
	
	m = struct.pack("xxB?" + str(l) + "s" + ("x" * (32 - 4 - l)),l,end,msg[:l])
	sock.sendto(m, (UDP_IP,UDP_PORT))
	
	msg = msg[l:]