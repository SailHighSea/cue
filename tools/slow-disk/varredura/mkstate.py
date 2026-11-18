# mkstate.py <out> <win path>...  : a state.json with these files as recents (newest first)
import sys, json, time
paths=sys.argv[2:]; now=int(time.time())
media=[{"path":p,"pos":30.0,"dur":180.0,"last":now-i*60} for i,p in enumerate(paths)]
st={"schema":5,"prefs":{"onboarded":True,"ambient":True,"resume":True,"news_seen":"1.4","news_closed":"1.4","installed":now-86400*30,"survey":"answered"},
    "session":{},"media":media}
open(sys.argv[1],"w").write(json.dumps(st,indent=1))
