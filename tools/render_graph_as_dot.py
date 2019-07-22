#!/usr/bin/env python3

from graph.load_xml import load_graph
from graph.snapshots import extractSnapshotInstances
import sys
import os
import argparse
import re
import math

parser=argparse.ArgumentParser("Render a graph to dot.")
parser.add_argument('graph', metavar="G", default="-", nargs="?")
parser.add_argument('--device-type-filter', dest="deviceTypeFilter", default=".*")
parser.add_argument('--snapshots', dest="snapshots", default=None)
parser.add_argument('--bind-dev',dest="bind_dev",metavar=("idFilter","property|state|rts","name","attribute","expression"), nargs=5,action="append",default=[])
parser.add_argument('--bind-edge',dest="bind_edge",metavar=("idFilter","property|state|firings","name","attribute","expression"), nargs=5,action="append",default=[])
parser.add_argument('--output', default="graph.dot")

args=parser.parse_args()

shapes=["oval","box","diamond","pentagon","hexagon","septagon", "octagon"]
colors=["blue4","red4","green4"]

reDeviceTypeFilter=re.compile(args.deviceTypeFilter)

deviceTypeToShape={}
edgeTypeToColor={}

if args.graph=="-":
    graph=load_graph(sys.stdin, "<stdin>")
else:
    graph=load_graph(args.graph, args.graph)


bind_dev=[]
for b in args.bind_dev:
    fd=exec("def bind_f(value): return {}".format(b[4]), globals(),locals())
    f=bind_f
    bind_dev.append( (b[0], b[1], b[2], b[3], f ) )

bind_edge=[]
for b in args.bind_edge:
    fd=exec("def bind_f(value): return {}".format(b[4]), globals(),locals())
    f=bind_f
    bind_edge.append( (b[0], b[1], b[2], b[3], f ) )


if len(graph.graph_type.device_types) <= len(shapes):
    for id in graph.graph_type.device_types.keys():
        deviceTypeToShape[id]=shapes.pop(0)
else:
    for id in graph.graph_type.device_types.keys():
        deviceTypeToShape[id]=shapes[0]

if len(graph.graph_type.message_types) <= len(colors):
    for id in graph.graph_type.message_types.keys():
        edgeTypeToColor[id]=colors.pop(0)
else:
    for id in graph.graph_type.message_types.keys():
        edgeTypeToColor[id]="black"


def blend_colors(colStart,colEnd,valStart,valEnd,val):
    if val<valStart:
        return color_to_str(colStart)
    elif val>valEnd:
        return color_to_str(colEnd)
    else:
        interp=(val-valStart)/(valEnd-valStart)
        if len(colStart)==3:
            return color_to_str( (
                colStart[0] + (colEnd[0]-colStart[0]) * interp,
                colStart[1] + (colEnd[1]-colStart[1]) * interp,
                colStart[2] + (colEnd[2]-colStart[2]) * interp,
            ) )
        else:
            return color_to_str( (
                colStart[0] + (colEnd[0]-colStart[0]) * interp,
                colStart[1] + (colEnd[1]-colStart[1]) * interp,
                colStart[2] + (colEnd[2]-colStart[2]) * interp,
                colStart[3] + (colEnd[3]-colStart[3]) * interp
            ) )

def color_to_str(col):
    def clamp(x):
        i=int(x)
        if x<0: return 0
        if x>255: return 255
        return i
    if len(col)==3:
        return "#{:02x}{:02x}{:02x}".format(clamp(col[0]),clamp(col[1]),clamp(col[2]))
    else:
        return "#{:02x}{:02x}{:02x}{:02x}".format(clamp(col[0]),clamp(col[1]),clamp(col[2]),clamp(col[3]))

def heat(valLow,valHigh,value):
    valMid=(valLow+valHigh)/2.0
    if value < valMid:
        return blend_colors( (0,0,255), (255,255,255), valLow, valMid, value)
    else:
        return blend_colors( (255,255,255), (255,0,0), valMid, valHigh, value)

def cycle(val, wrap):
    val=math.floor(val)
    val=val%wrap
    val=val/(wrap+1)
    
    return color_to_str( ( 0, math.floor(val*255), 0 ) )


def write_graph(dst, graph,devStates=None,edgeStates=None):
    def out(string):
        print(string,file=dst)

    minFirings={}
    maxFirings={}
    if edgeStates:
        for (id,es) in edgeStates.items():
            et=graph.edge_instances[id].message_type.id
            firings=int(es[1])
            minFirings[et]=min(firings,  minFirings.get(et,firings))
            maxFirings[et]=max(firings,  maxFirings.get(et,firings))

        sys.stderr.write("min = {}, max = {}\n".format(minFirings,maxFirings))

    out('digraph "{}"{{'.format(graph.id))
    out('  sep="+10,10";');
    out('  overlap=false;');
    out('  spline=true;');
    
    incNodes=set()

    for di in graph.device_instances.values():
        dt=di.device_type
        
        if not reDeviceTypeFilter.match(dt.id):
            continue
        
        incNodes.add( di.id )

        if devStates:
            stateTuple=devStates.get(di.id,None)
            if stateTuple:
                state=stateTuple[0]
                rts=stateTuple[1]


        props=[]
        shape=deviceTypeToShape[di.device_type.id]
        props.append('shape={}'.format(shape))

        meta=di.metadata
        if meta and "x" in meta and "y" in meta:
            props.append('pos="{},{}"'.format(meta["x"],meta["y"]))
            props.append('pin=true')

        for b in bind_dev:
            #print("Type : {}".format(di.device_type.state))
            #print("Pre-expand : {}".format(state))
            state=di.device_type.state.expand(state)
            #print("Post-expand : {}".format(state))

            if b[0]=="*" or b[0]==di.device_type.id:
                if b[1]=="property":
                    value=di.properties[b[2]]
                elif b[1]=="state":
                    if b[2] not in state:
                        raise RuntimeError("No state element called {} in device {} of device type {}. State = {}".format(b[2],di.id,di.device_type.id, state))
                    value=state[b[2]]
                elif b[1]=="rts":
                    value=rts
                else:
                    raise RuntimeError("Must specify either property or state.")
                strValue=b[4](value)
                props.append('{}="{}"'.format(b[3],strValue))
                props.append('style="filled"')

        out('  "{}" [{}];'.format(di.id, ",".join(props)))

    addLabels=len(graph.edge_instances) < 50

    for ei in graph.edge_instances.values():
        if not ei.src_device.id in incNodes:
            continue
        if not ei.dst_device.id in incNodes:
            continue
        
        if edgeStates:
            stateTuple=edgeStates.get(ei.id,None)
            if stateTuple:
                state=stateTuple[0]
                firings=stateTuple[1]
                queue=stateTuple[2]

        props={}
        props["color"]=edgeTypeToColor[ei.message_type.id]
        if addLabels:
            props["headlabel"]=ei.dst_pin.name
            props["taillabel"]=ei.src_pin.name
            props["label"]=ei.message_type.id

        for b in bind_edge:
            if b[0]=="*" or b[0]==ei.message_type.id:
                if b[1]=="property":
                    value=di.properties[b[2]]
                elif b[1]=="state":
                    value=state[b[2]]
                elif b[1]=="firings":
                    mn=float(minFirings[b[0]])
                    mx=float(maxFirings[b[0]])
                    m=float(firings)
                    if mn-mx==0:
                        value=0
                    else:
                        value=(m-mn)/(mx-mn)
#                    sys.stderr.write("  min = {}, max={}, m={}, value={}\n".format(mn,mx,m,value))
                else:
                    raise RuntimeError("Must specify either property or state.")
                strValue=b[4](value)
                props[b[3]]=strValue

        props=",".join([ '{}="{}"'.format(k,v) for (k,v) in props.items()])

        out('  "{}" -> "{}" [{}];'.format(ei.src_device.id,ei.dst_device.id,props))


    out("}")

if args.snapshots==None:
    dst=sys.stdout
    if args.output!="-":
        dst=open(args.output,"wt")
    write_graph(dst,graph)
else:
    dstPath=args.output
    if dstPath=="-":
        raise RuntimeError("Output path can't be stdout for snapshots, as multiple files are needed.")

    edgeFirings={ ei.id : 0 for ei in graph.edge_instances.values() }

    def onSnapshot(graphType,graphInst,orchTime,seqNum,devStates,edgeStates):
        print("graphInst = {}, orchTime = {}, seqNum = {}, numdevs = {}, numedges = {}\n".format(
            graphInst.id, orchTime, seqNum, len(graphInst.device_instances), len(graphInst.edge_instances))
        )
        with open("{}_{:06d}.dot".format(dstPath,int(seqNum)),"wt") as dst:
            write_graph(dst,graphInst,devStates,edgeStates)
    graphTypes={ graph.graph_type.id:graph.graph_type }
    graphInstances = {graph.id:graph}
    extractSnapshotInstances(graphInstances,args.snapshots,onSnapshot)
