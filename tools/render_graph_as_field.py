from graph.load_xml import load_graph
from graph.snapshots import extractSnapshotInstances
import sys
import os
import argparse

from PIL import Image

parser=argparse.ArgumentParser("Render a graph to bitmap fields.")
parser.add_argument('graph', metavar="G")
parser.add_argument('snapshots', metavar="S")
parser.add_argument('--bind-dev',dest="bind_dev",metavar=("idFilter","property|state|rts","name","expression"), nargs=4,action="append",default=[])
parser.add_argument('--output-prefix', dest="output_prefix", default="graph")

args=parser.parse_args()

if args.graph!="-":
    basePath=os.path.dirname(args.graph)
else:
    basePath=os.getcwd()

graph=load_graph(args.graph,basePath)

if "location.dimension" not in graph.graph_type.metadata:
    raise RuntimeError("This graph doesn't seem to be a 2d field.")

scaleX=1
scaleY=1

minX=100000000000000
maxX=-100000000000000
minY=100000000000000
maxY=-100000000000000

for di in graph.device_instances.values():
    if not "loc" in di.metadata:
        continue
    loc=di.metadata["loc"]
    minX=min(minX,loc[0])
    minY=min(minY,loc[1])
    maxX=max(maxX,loc[0])
    maxY=max(maxY,loc[1])

width=int((maxX-minX+1)*scaleX)
height=int((maxX-minX+1)*scaleX)

bind_dev=[]
for b in args.bind_dev:
    fd=exec("def bind_f(value): return {}".format(b[3]), globals(),locals())
    f=bind_f
    bind_dev.append( (b[0], b[1], b[2],  f ) )


def blend_colors(colStart,colEnd,valStart,valEnd,val):
    if val<valStart:
        return colStart
    elif val>valEnd:
        return colEnd
    else:
        interp=(val-valStart)/(valEnd-valStart)
        return    (
                colStart[0] + (colEnd[0]-colStart[0]) * interp,
                colStart[1] + (colEnd[1]-colStart[1]) * interp,
                colStart[2] + (colEnd[2]-colStart[2]) * interp, 
            )

def heat(valLow,valHigh,value):
    valMid=(valLow+valHigh)/2.0
    if value < valMid:
        return blend_colors( (0,0,255), (255,255,255), valLow, valMid, value)
    else:
        return blend_colors( (255,255,255), (255,0,0), valMid, valHigh, value)
    
        
def render_graph(dstPath, graph,devStates):
    print( "width = {}, height = {}".format(width,height))
    
    im=Image.new("RGB", (width,height) )
    
    for di in graph.device_instances.values():
        
        
        stateTuple=devStates.get(di.id,None)
        assert(stateTuple)
        state=stateTuple[0]
        rts=stateTuple[1]
        
        if "loc" not in di.metadata:
            continue
        
        loc=di.metadata["loc"]
        x=(loc[0]-minX)*scaleX
        y=(loc[1]-minY)*scaleY

        value=None

        for b in bind_dev:
            if b[0]=="*" or b[0]==di.device_type.id:
                if b[1]=="property":
                    value=di.properties[b[2]]
                elif b[1]=="state":
                    value=state[b[2]]
                elif b[1]=="rts":
                    value=rts
                else:
                    raise RuntimeError("Must specify either property or state.")
                value=b[3](value)
                break
            
        if value:
            im.putpixel( (int(x),int(y)), (int(value[0]),int(value[1]),int(value[2])) )

    im.save(dstPath)
            
            
dstPath=args.output_prefix


def onSnapshot(graphType,graphInst,orchTime,seqNum,devStates,edgeStates):
    print("graphInst = {}, orchTime = {}, seqNum = {}, numdevs = {}, numedges = {}\n".format(
        graphInst.id, orchTime, seqNum, len(graphInst.device_instances), len(graphInst.edge_instances))
    )

    render_graph("{}_{:06d}.png".format(dstPath,int(seqNum)) ,graphInst,devStates)

graphTypes={ graph.graph_type.id:graph.graph_type }
graphInstances = {graph.id:graph}
extractSnapshotInstances(graphInstances,args.snapshots,onSnapshot)
