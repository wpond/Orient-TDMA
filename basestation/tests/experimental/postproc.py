import sys

id = sys.argv[1]

i = open("test%s-results.csv" % id,"r")
o = open("test%s-proc.csv" % id,"w")

lastkey = ()
stats = {}
keys = []
for l in i:
	(testid,repeat,nodeid,duration,nodecount,txp,loss,optimisations,speedspread,datarate,slots,lease,throughput,hits,misses) = l.split(",")
	key = "%s,%s,%s,%s,%s,%s,%s,%s" % (testid,nodecount,txp,loss,optimisations,speedspread,slots,lease)
	if not key in stats:
		stats[key] = {}
	if not nodeid in stats[key]:
		stats[key][nodeid] = {
			"hits": 0,
			"misses": 0,
			"throughput": 0,
			"count": 0,
		}
	stats[key][nodeid]["hits"] += int(hits)
	stats[key][nodeid]["misses"] += int(misses)
	stats[key][nodeid]["throughput"] += float(throughput)
	stats[key][nodeid]["count"] += 1

for key in stats:
	summary = key
	print key
	for nodeid in stats[key]:
		avghits = stats[key][nodeid]["hits"] / float(stats[key][nodeid]["count"])
		avgmisses = stats[key][nodeid]["misses"] / float(stats[key][nodeid]["count"])
		avgthroughput = stats[key][nodeid]["throughput"] / float(stats[key][nodeid]["count"])
		summary += ",%s,%s,%s" % (avgthroughput,avghits,avgmisses)
	o.write(summary + "\n")

i.close()
