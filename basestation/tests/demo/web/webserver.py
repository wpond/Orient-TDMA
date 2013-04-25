#Copyright Jon Berg , turtlemeat.com

import string,cgi,time
from os import curdir, sep
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
import socket, thread

UDP_IP = "0.0.0.0"
UDP_PORT = 9899

class Log():
	log = ""
	testSet = False
	testId = 0
	repeatId = 0
	repeats = 0
	time = 0
	nodeCount = 0
	txp = 0
	loss = 0
	optimisations = 0
	dataRate = 0
	slots = 0
	lease = 0
	spreadspeed = 0
	packets = {}
	misses = {}
	
	def reset(self):
		self.testSet = False
		self.log = ""
		self.testId = 0
		self.repeatId = 0
		self.repeats = 0
		self.time = 0
		self.nodeCount = 0
		self.txp = 0
		self.loss = 0
		self.optimisations = 0
		self.dataRate = 0
		self.slots = 0
		self.lease = 0
		self.spreadspeed = 0
		self.packets = {}
		self.misses = {}
		self.log += "Connected<br />"
		
	def finish(self):
		self.testSet = False
		self.log += "Disconnected<br />"
	
	def nextTest(self):
		self.repeatId += 1
		self.packets = {}
		self.misses = {}
	
	def setParams(self,testId, repeats, time, nodeCount, txp, loss, optimisations, dataRate, slots, lease, spreadspeed):
		self.testId = testId
		self.repeats = repeats
		self.testSet = True
		self.repeatId = 0
		self.time = time
		self.nodeCount = nodeCount
		self.txp = txp
		self.loss = loss
		self.optimisations = optimisations
		self.dataRate = dataRate
		self.slots = slots
		self.lease = lease
		self.spreadspeed = spreadspeed
		self.packets = {}
		self.misses = {}
	
	def recv(self,nid,frame,seg):
		if not nid in self.packets:
			self.packets[nid] = []
		self.packets[nid].append({ "frame" : frame, "seg" : seg })
	
	def miss(self,nid,frame,seg):
		if not nid in self.misses:
			self.misses[nid] = []
		self.misses[nid].append({ "frame" : frame, "seg" : seg })
	
log = Log()

def recvr(log):
	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	sock.settimeout(1)
	sock.bind((UDP_IP,UDP_PORT))
	while True:
		try:
			data,addr = sock.recvfrom(128)
		except socket.timeout:
			continue
		
		if data == "start":
			log.reset()
		elif data == "end":
			log.finish()
		elif data == "teststart":
			log.nextTest()
		elif data.startswith("parameters:"):
			params = data.split(":")[1].split(",")
			try:
				log.setParams(params[0],params[1],params[2],params[3],params[4],params[5],params[6],params[7],params[8],params[9],params[10])
			except:
				print "found that exception, output: %s, %s" % (data,params)
		elif data.startswith("recv:"):
			details = data.split(":")[1].split("/")
			log.recv(details[0],details[1],details[2])
		elif data.startswith("miss:"):
			details = data.split(":")[1].split("/")
			log.miss(details[0],details[1],details[2])
		
		data = data.replace("\n","<br />")
		print "%s[SERVER] - %s" % (addr[0],data)
		log.log += data + "<br />"

class MyHandler(BaseHTTPRequestHandler):

	def do_GET(self):
		self.send_response(200)
		self.send_header('Content-type',	'text/html')
		self.end_headers()
		
		if log.testSet:
			body = "<h1>Honours Project Visualisation</h1><h2>Test Details</h2>Test ID: %s (%s of %s repeats)<br />Duration: %s seconds<br />Node Count: %s<br />Transmit Period: %s<br />" % (log.testId,log.repeatId,log.repeats,log.time,log.nodeCount,log.txp)
		else:
			body = "<h1>Honours Project Visualisation</h1><h2>Test Details</h2><em>No test configured</em>"
		
		body += "<h2>Packet Details</h2>"
		if len(log.packets) > 0:
			for nid in log.packets:
				total = len(log.packets[nid])
				hits = total
				if nid in log.misses:
					misses = len(log.misses[nid])
					total += misses
				else:
					misses = 0
				body += "<h3>Node %s</h3>Packets received: %s<br />Hits: %s<br />Misses: %s<br />Goodput: %f<br />" % (nid,total,hits,misses,float(hits)/float(total))
		
		else:
			body += "<em>No packets received yet</em>"
		
		self.wfile.write("<html><head><title>Honours Project Visualiser</title><meta http-equiv=\"refresh\" content=\"1;url=http://localhost:2020/\"></head><body>" + 
			body + 
			"</body></html>")
	
	def do_POST(self):
		pass

def main():
	try:
		server = HTTPServer(('', 2020), MyHandler)
		print 'started httpserver...'
		thread.start_new_thread(recvr,(log,))
		server.serve_forever()
	except KeyboardInterrupt:
		print '^C received, shutting down server'
		server.socket.close()

if __name__ == '__main__':
    main()

