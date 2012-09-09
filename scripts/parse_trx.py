import sys

ins = open( sys.argv[1], "r" )
array = {}
if len(sys.argv)>=2:
	pref=sys.argv[2]
else:
	pref=""
for line in ins:
	data=line.split()
	if array.has_key(data[3]):
		array[data[3]] = array[data[3]]+1
	else:
		array[data[3]] = 1

minsec=int(min(array.keys()))
maxsec=int(max(array.keys()))
i=minsec
while i<=maxsec:
	if array.has_key(str(i)):
		print pref,i-minsec,array[str(i)]
	else:
		print pref,i-minsec,0
	i=i+1
