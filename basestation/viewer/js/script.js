
$(document).ready(main);

function main()
{
	// populate slots
	for (slotIdx = 0; slotIdx < 10; slotIdx++)
	{
		$("#slots").append("<div id=\"slot-" + slotIdx + "\"class=\"slot unreserved\">" + slotIdx + "</div>");
	}
	
	for (nodeIdx = 0; nodeIdx < 5; nodeIdx++)
	{
		$("#nodes").append(createNodeHtml(nodeIdx));
		reserveSlot(nodeIdx);
	}
}

function reserveSlot(slotIdx)
{
	$("#slots #slot-" + nodeIdx).addClass("reserved").removeClass("unreserved");
}

function rand(min,max)
{
	range = max - min;
	return Math.floor((Math.random()*range)+min)
}

function createNodeHtml(id)
{
	s = "<div id=\"node-" + id + "\" class=\"node\"><h2>Node " + id + "</h2>";
	c = 0;
	t = rand(30,50);
	for (packetIdx = c; packetIdx < t+c; packetIdx++)
	{
		s += "<div id=\"packet-" + packetIdx + "\"class=\"packet recvd\"></div>";
	}
	c = t
	t = rand(5,20);
	for (packetIdx = c; packetIdx < t+c; packetIdx++)
	{
		s += "<div id=\"packet-" + packetIdx + "\"class=\"packet dropped\"></div>";
	}
	for (packetIdx = c+t; packetIdx < 100; packetIdx++)
	{
		s += "<div id=\"packet-" + packetIdx + "\"class=\"packet waiting\"></div>";
	}
	s += "</div>"
	return s;
}
