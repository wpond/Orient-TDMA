import sys

if len(sys.argv) < 2:
	print "include log input"
	sys.exit(0)

i = open(sys.argv[1],"r")
o = open("out.txt","w")

for l in i:
	o.write(l.replace("\n","\\newline\n"))

i.close()
o.close()
