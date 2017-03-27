

def merge_metadata(base,new):
    """Meta data is merged at the key-value level. Everything else
       is copied through. The rules are:
       
       # Keep key/value
       base = {key:oVal},  new = {}          ->  res = { key:oVal}
       
       # Merge key/value
       base = {key:oVal},  new = {key:nVal}  ->  res = { key:merge_metadata(oVal,nVal) }
       
       # Delete key/value
       base = {key:oVal},  new = {key:None}  ->  res = { } 
       
       # Add key/value
       base = {},          new = {key:nVal}  ->  res = { key:nVal}
    """
    
    assert isinstance(base,dict)
    assert isinstance(new,dict)
        
    for (k,nv) in new.items():
        if k in base:
            ov=base[k]
            if nv is None:
                del base[k]
            elif isinstance(dict(nv)) and isinstance(dict(ov)):
                base[k]=merge_metadata(ov,nv)
            else:
                base[k]=nv
        else:
            base[k]=new[k]
    
    return base


def diff_metadata(base,new):
    assert isinstance(base,dict)
    assert isinstance(new,dict)
    
    diff={}
    
    for (k,ov) in old.items():
        if k in new:
            nv=new[k]
            if nv is None:
                diff[nv
            


class GraphInstanceMetadata:
    def __init__(self, graphId, graphInstance, devInstances={}, edgeInstances={}):
        self.graph_id=graphId
        self.graph_instance=graphInstance
        assert isinstance(devInstances,dict)
        self.device_instances=devInstances
        assert isinstance(devInstances,dict)
        self.edge_instances=edgeInstances
        
    def apply_to_graph(self,graph):
        assert graph.id==self.graph_id
        graph.metadata=merge_metadata(graph.metadata,self.graph_instance)
        for (di,m) in self.device_instances:
            graph.device_instances[di].metadata=merge_metadata(graph.device_instances[di].metadata,m)
        for (ei,m) in self.edge_instances:
            graph.edge_instances[ei].metadata=merge_metadata(graph.edge_instances[ei].metadata,m)
    
    def create_device_instance_key(self,keyTag):
        deviceKeys=self.graph_instance.setdefault("device.keys", dict())
        keyIdx=0
        key=str(keyIdx)
        while key in deviceKeys:
            keyIdx+=1
            key=str(keyIdx)
        deviceKeys[key]=keyTag
        return key
        
    def remove_device_instance_key(self,key):
        assert "device_keys" in self.graph_instance
        assert key in self.graph_instance["device.keys"]
        del self.graph_instance["device.keys"][key]
        for (di,m) in self.device_instances:
            if key in m:
                m[key]=None
        
    def create_edge_instance_key(self,keyTag):
        deviceKeys=self.graph_instance.setdefault("edge.keys", dict())
        keyIdx=0
        key=str(keyIdx)
        while key in deviceKeys:
            keyIdx+=1
            key=str(keyIdx)
        deviceKeys[key]=keyTag
        return key
    
def load_metadata_patch(
