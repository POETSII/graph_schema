from graph.load_xml import load_graph
from graph.snapshots import extractSnapshotInstances
import sys
import os
import argparse

parser=argparse.ArgumentParser("Render a graph to dot.")
parser.add_argument('graph', metavar="G", default="-", nargs="?")
parser.add_argument('--snapshots', dest="snapshots", default=None)
parser.add_argument('--bind-dev',dest="bind_dev",metavar=("idFilter","property|state|rts","name","attribute","expression"), nargs=5,action="append",default=[])
parser.add_argument('--output', default="graph.dot")

args=parser.parse_args()

shapes=["oval","box","diamond","pentagon","hexagon","septagon", "octagon"]
colors=["blue4","red4","green4"]

deviceTypeToShape={}
edgeTypeToColor={}

if args.graph=="-":
    graph=load_graph(sys.stdin)
else:
    graph=load_graph(args.graph)


bind_dev=[]
for b in args.bind_dev:
    fd=exec("def bind_f(value): return {}".format(b[4]), globals(),locals())
    f=bind_f
    bind_dev.append( (b[0], b[1], b[2], b[3], f ) )

    
if len(graph.graph_type.device_types) <= len(shapes):
    for id in graph.graph_type.device_types.keys():
        deviceTypeToShape[id]=shapes.pop(0)
else:
    for id in graph.graph_type.device_types.keys():
        deviceTypeToShape[id]=shapes[0]

if len(graph.graph_type.edge_types) <= len(colors):
    for id in graph.graph_type.edge_types.keys():
        edgeTypeToColor[id]=colors.pop(0)
else:
    for id in graph.graph_type.edge_types.keys():
        edgeTypeToColor[id]="black"


def blend_colors(colStart,colEnd,valStart,valEnd,val):
    if val<valStart:
        return color_to_str(colStart)
    elif val>valEnd:
        return color_to_str(colEnd)
    else:
        interp=(val-valStart)/(valEnd-valStart)
        return color_to_str( (
            colStart[0] + (colEnd[0]-colStart[0]) * interp,
            colStart[1] + (colEnd[1]-colStart[1]) * interp,
            colStart[2] + (colEnd[2]-colStart[2]) * interp, 
            ) )

def color_to_str(col):
    def clamp(x):
        i=int(x)
        if x<0: return 0
        if x>255: return 255
        return i
    return "#{:02x}{:02x}{:02x}".format(clamp(col[0]),clamp(col[1]),clamp(col[2]))
        
        
def write_graph(dst, graph,devStates=None,edgeStates=None):
    def out(string):
        print(string,file=dst)
    
    out('digraph "{}"{{'.format(graph.id))
    out('  sep="+10,10";');
    out('  overlap=false;');
    out('  spline=true;');
    
    for di in graph.device_instances.values():
        dt=di.device_type
        
        stateTuple=devStates.get(di.id,None)
        if stateTuple:
            state=stateTuple[0]
            rts=stateTuple[1]
        

        props=[]        
        shape=deviceTypeToShape[di.device_type.id]
        props.append('shape={}'.format(shape))
        
        pos=di.native_location
        if pos:
            props.append('pos="{},{}"'.format(pos[0]*0.25,pos[1]*0.25))

        for b in bind_dev:
            if b[0]=="*" or b[0]==di.device_type.id:
                assert di.device_type.id==b[0]
                if b[1]=="property":
                    value=di.properties[b[2]]
                elif b[1]=="state":
                    value=state[b[2]]
                elif b[1]=="rts":
                    value=rts
                else:
                    raise RuntimeError("Must specify either property or state.")
                strValue=b[4](value)
                props.append('{}="{}"'.format(b[3],strValue))
                props.append('style="filled"')
                

        #if state and di.device_type.id=="branch":
        #    # sys.stderr.write("  {}\n".format(state[0]))
        #    col=blend_colors( (255,255,0), (255,0,255), 0, 1, state[0]["pending"])
        #    col=color_to_str(col)
        #    props.append('style="filled",fillcolor="{}"'.format(col))
            

        out('  "{}" [{}];'.format(di.id, ",".join(props)))

    addLabels=len(graph.edge_instances) < 50

    for ei in graph.edge_instances.values():
        color=edgeTypeToColor[ei.edge_type.id]
        if addLabels:
            out('  "{}" -> "{}" [ headlabel="{}", taillabel="{}", label="{}", color="{}" ];'.format(ei.src_device.id,ei.dst_device.id,ei.dst_port.name,ei.src_port.name, ei.edge_type.id, color ))
        else:
            out('  "{}" -> "{}" [ color="{}" ];'.format(ei.src_device.id,ei.dst_device.id, color ))

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
    def onSnapshot(graphType,graphInst,orchTime,seqNum,devStates,edgeStates):
        print("graphInst = {}, orchTime = {}, seqNum = {}, numdevs = {}, numedges = {}\n".format(
            graphInst.id, orchTime, seqNum, len(graphInst.device_instances), len(graphInst.edge_instances))
        )
        with open("{}_{:06d}.dot".format(dstPath,int(seqNum)),"wt") as dst:
            write_graph(dst,graphInst,devStates,edgeStates)
    graphTypes={ graph.graph_type.id:graph.graph_type }
    graphInstances = {graph.id:graph}
    extractSnapshotInstances(graphInstances,args.snapshots,onSnapshot)
