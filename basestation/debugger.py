import socket,struct

class Debugger():
	
	UDP_IP = "127.0.0.1"
	UDP_PORT = 9899
	
	def __init__(self):
		self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		self.sendMsg("\n\n=====\n\n\n")
	
	def sendMsg(self,msg):
		end = False
		while not end:
			if len(msg) > 28:
				l = 28
				end = False
			else:
				l = len(msg)
				end = True
			
			m = struct.pack("xxB?" + str(l) + "s" + ("x" * (32 - 4 - l)),l,end,msg[:l])
			self.sock.sendto(m, (self.UDP_IP,self.UDP_PORT))
	
	def send(self,packet):
		self.sock.sendto(packet, (self.UDP_IP,self.UDP_PORT))
		
	def close(self):
		self.sock.close()