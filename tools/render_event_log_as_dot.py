#!/usr/bin/env python3

from graph.events import *
import sys
import os
import argparse

parser=argparse.ArgumentParser("Render an event log to dot.")
parser.add_argument('graph', metavar="G", default="-", nargs="?")
parser.add_argument('--output', default="graph.dot")

args=parser.parse_args()    
        
def write_graph(dst, src):
    def out(string):
        #sys.stderr.write("{}\n".format(string))
        print(string,file=dst)
        
    eventsBySeq={} # Map of deviceId : [ (seq,eventId) ]

    eventsByBarrier={} # Map of barrierId : [eventId]
    
    class LogSink(LogWriter):
        def onInitEvent(self,e):
            eventsBySeq.setdefault(e.dev,[]).append( (e.seq,e) )
        
        def onSendEvent(self,e):
            eventsBySeq.setdefault(e.dev,[]).append( (e.seq,e) )
        
        def onRecvEvent(self,e):
            out('  "{}" -> "{}";'.format(e.sendEventId,e.eventId))
            eventsBySeq.setdefault(e.dev,[]).append( (e.seq,e) )

        def onHardwareIdleEvent(self,e):
            eventsBySeq.setdefault(e.dev,[]).append( (e.seq,e) )
            eventsByBarrier.setdefault(e.barrierId,[]).append(e)

        def onDeviceIdleEvent(self,e):
            eventsBySeq.setdefault(e.dev,[]).append( (e.seq,e) )


    sink=LogSink()
    
    out('digraph "{}"{{'.format("graph"))
    #out('  sep="+10,10";');
    out('  overlap=false;');
    #out('  spline=true;');
    out('  newrank=true;');
    
    parseEvents(src,sink)
    
    def makeState(curr,prev,order):
        for (k,v) in prev.items():
            if k not in curr:
                curr[k]=0
                
        for (k,v) in curr.items():
            if k not in prev:
                order.append(k)
        
                
        res=""
        for k in order:
            cv=curr[k]
            pv=prev.get(k,None)
            if isinstance(cv,list):
                if len(cv)>64:
                    cv="["+",".join([str(x) for x in cv[:32]])+",...]"
            if isinstance(pv,list):
                if len(pv)>64:
                    pv="["+",".join([str(x) for x in pv[:32]])+",...]"
            if cv==pv:
                res+="<TR><TD>{}</TD><TD>{}</TD></TR>".format(k,  cv)
            else:
                res+="<TR><TD>{}</TD><TD color='red'>{} / {}</TD></TR>".format(k, pv, cv)
        return res
        
    def makeMessage(m):
        if m is None:
            return
        vv=sorted( list( m.items() ) )
        
        res=""
        for (k,v) in vv:
            res+="<TR><TD>{}</TD><TD>{}</TD></TR>".format(k,v)
        return res
    
    for (dev,events) in eventsBySeq.items():
        out('  subgraph cluster_{} {{'.format(dev))
        order=[e for (s,e) in sorted(events)]
        ko=[]
        prev={}
        for e in order:
            content=""
            curr={k:v for (k,v) in e.S.items() }
            content='"{}" [ shape=none, margin=0, label=< <TABLE>'.format(e.eventId)
            if e.type=="init":
                content+='<TR ><TD bgcolor="blue" colspan="2">{} : __init__</TD></TR>'.format(e.dev)
            elif e.type=="send":
                content+='<TR><TD bgcolor="green" colspan="2">{} : Send : {}</TD></TR>'.format(e.dev,e.pin)
            elif e.type=="recv":
                content+='<TR><TD bgcolor="orange" colspan="2">{} : Recv : {}</TD></TR>'.format(e.dev,e.pin)
            elif e.type=="hardware_idle":
                content+='<TR><TD bgcolor="yellow" colspan="2">{} : HwIdle</TD></TR>'.format(e.dev)
            elif e.type=="device_idle":
                content+='<TR><TD bgcolor="purple" colspan="2">{} : DevIdle</TD></TR>'.format(e.dev)
            else:
                raise RuntimeError("Unknown type.")
            content+=makeState(curr,prev,ko)
            if e.type=="send":
                content+='<TR><TD colspan="2">Message</TD></TR>'
                if e.M:
                    content+=makeMessage(e.M)
            content+='</TABLE> > ];'
            out(content)
            prev=curr
        for i in range(1,len(order)):
            out('    "{}" -> "{}" [ style="dotted" ] ;'.format(order[i-1].eventId,order[i].eventId))
        out('  }')

    for (bid,events) in eventsByBarrier.items():
        out('  subgraph barrier_{} {{'.format(bid))
        out('    rank=same;')
        for e in events:
            out('    "{}";'.format(e.eventId))
        out('  }')
    out("}")

dst=sys.stdout
if args.output!="-":
    sys.stderr.write("Opening output file {}\n".format(args.output))
    dst=open(args.output,"wt")
write_graph(dst,args.graph)
