

def merge_metadata(base,new):
    """
        The dictionary base is updated in-place.
    
        Meta data is merged at the key-value level. Everything else
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
    
    assert isinstance(base,dict), "Expected base to be dict, got {}".format(base)
    assert isinstance(new,dict)
        
    for (k,nv) in new.items():
        if k in base:
            ov=base[k]
            if nv is None:
                del base[k]
            elif isinstance(nv,dict) and isinstance(ov,dict):
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
    
    for (k,ov) in base.items():
        if k in new:
            nv=new[k]
            if nv is None:
                diff[k]=None
            elif isinstance(ov,dict) and isinstance(nv,dict):
                diff[k]=diff_metadata(ov,nv)
            elif nv==ov:
                pass
            else:
                diff[k]=nv
        else:
            diff[k]=None
    
    for (k,nv) in new.items():
        if k not in base:
            diff[k]=nv
    
    return diff
            
def patch_metadata(base, diff):
    assert isinstance(base,dict)
    assert isinstance(diff,dict)
    
    # patching is actually the same as merging
    return merge_metadata(base,diff)

        
def merge_metadata_into_graph(graph, graphMeta, deviceMeta, edgeMeta):
    graph.metadata=merge_metadata(graph.metadata or {},graphMeta or {})
    for (di,m) in deviceMeta.items():
        graph.device_instances[di].metadata=merge_metadata(graph.device_instances[di].metadata or {},m)
    for (ei,m) in edgeMeta.items():
        graph.edge_instances[ei].metadata=merge_metadata(graph.edge_instances[ei].metadata or {},m)
    return graph

def diff_metadata_against_graph(graphMeta, deviceMeta, edgeMeta, graph):
    diffGraphMeta=diff_metadata(graph.metadata, graphMeta)
    
    diffDeviceMeta={}
    for (di,ov) in graph.device_instances.items():
        if di in deviceMeta:
            nv = deviceMeta[di]
            if nv==None:
                diffDeviceMeta[di]=None
            else:
                diffDeviceMeta[di]=diff_metadata(ov,nv)
        else:
            pass
    
    for (di,nv) in deviceMeta:
        assert di in graph.device_instances
        
    diffEdgeMeta={}
    for (di,ov) in graph.edge_instances.items():
        if di in edgeMeta:
            nv = edgeMeta[di]
            if nv==None:
                diffEdgeMeta[di]=None
            else:
                diffEdgeMeta[di]=diff_metadata(ov,nv)
        else:
            pass
    
    for (di,nv) in edgeMeta:
        assert di in graph.edge_instances

    return (diffGraphMeta,diffDeviceMeta,diffEdgeMeta)
    
          
def create_device_instance_key(keyTag,graphMetadata):
    deviceKeys=graphMetadata.setdefault("device.keys", dict())
    keyIdx=0
    key="k"+str(keyIdx)
    while key in deviceKeys:
        keyIdx+=1
        key="k"+str(keyIdx)
    return (key,{"device.keys" : { key : keyTag } } )
    
def remove_device_instance_key(key,graphMetadata,deviceMetadata):
    assert "device.keys" in graphMetadata
    assert key in graphMetadata["device.keys"]
    del graphMetadata["device.keys"][key]
    for (di,m) in deviceMetadata:
        if key in m:
            del m[key]


import unittest
import copy

class TestDiffPatchMetadata(unittest.TestCase):
    def test_merges(self):
        tests=[
            ( {}, {}, {} ),
            ( {"x":1}, {}, {"x":1} ),
            ( {}, {"x":1}, {"x":1} ),
            ( {"x":1}, {"x":1}, {"x":1} ),
            ( {"x":1}, {"x":2}, {"x":2} ),
            ( {"x":1}, {"x":"x"}, {"x":"x"} ),
            ( {"x":1}, {"x":{}}, {"x":{}} ),
            ( {"x":1}, {"x":{"a":1}}, {"x":{"a":1}} ),
            ( {"x":{"a":1}}, {"x":{}}, {"x":{"a":1}} ),
            ( {"x":{"a":1}}, {"x":{"a":56}}, {"x":{"a":56}} ),
            ( {"x":{"a":1}}, {"x":{"a":None}}, {"x":{}} ),
            ( {"x":{"a":1}}, {"x":None}, {} )
        ]
        for t in tests:
            base=copy.deepcopy(t[0])
            merge_metadata(base, t[1])
            self.assertEqual( base, t[2] )
            
    def test_diffs(self):
        tests=[
            ( {}, {}, {} ),
            ( {"x":1}, {}, {"x":None} ),
            ( {}, {"x":1}, {"x":1} ),
            ( {"x":1}, {"x":1}, {} ),
            ( {"x":1}, {"x":2}, {"x":2} ),
            ( {"x":1}, {"x":"x"}, {"x":"x"} ),
            ( {"x":1}, {"x":{}}, {"x":{}} ),
            ( {"x":1}, {"x":{"a":1}}, {"x":{"a":1}} ),
            ( {"x":{"a":1}}, {"x":{}}, {"x":{"a":None}} ),
            ( {"x":{"a":1}}, {"x":{"a":56}}, {"x":{"a":56}} ),
            ( {"x":{"a":1}}, {"x":{"a":None}}, {"x":{"a":None}} ),
            ( {"x":{"a":1}}, {"x":None}, {"x":None} )
        ]
        for t in tests:
            base=copy.deepcopy(t[0])
            diff=diff_metadata(base, t[1])
            self.assertEqual( diff, t[2], str(t) )
    
    def test_patch(self):
        tests=[
            ( {}, {}, {}, {} ),
            ( {"x":1}, {}, {"x":None}, {} ),
            ( {}, {"x":1}, {"x":1}, {"x":1} ),
            ( {"x":1}, {"x":1}, {}, {"x":1} ),
            ( {"x":1}, {"x":2}, {"x":2}, {"x":2} ),
            ( {"x":1}, {"x":"x"}, {"x":"x"}, {"x":"x"} ),
            ( {"x":1}, {"x":{}}, {"x":{}}, {"x":{}} ),
            ( {"x":1}, {"x":{"a":1}}, {"x":{"a":1}}, {"x":{"a":1}} ),
            ( {"x":{"a":1}}, {"x":{}}, {"x":{"a":None}}, {"x":{}} ),
            ( {"x":{"a":1}}, {"x":{"a":56}}, {"x":{"a":56}}, {"x":{"a":56}} ),
            ( {"x":{"a":1}}, {"x":{"a":None}}, {"x":{"a":None}}, {"x":{}} ),
            ( {"x":{"a":1}}, {"x":None}, {"x":None}, {} )
        ]
        for t in tests:
            base=copy.deepcopy(t[0])
            diff=diff_metadata(base, t[1])
            self.assertEqual( diff, t[2], str(t) )
            
            new=copy.deepcopy(t[0])
            patch_metadata(new, diff)
            self.assertEqual( new, t[3], str(t) )

class TestAddKey(unittest.TestCase):
    def test_add_key(self):
        gm={}
        k=create_device_instance_key("taggle",gm)
        self.assertEqual(gm["device.keys"][k], "taggle")
        
    def test_add_rem_key(self):
        gm={}
        k=create_device_instance_key("taggle",gm)
        self.assertEqual(gm["device.keys"][k], "taggle")
        remove_device_instance_key(k,gm,{})
        self.assertTrue(k not in gm["device.keys"])
        
    def test_add_two_keys(self):
        gm={}
        k1=create_device_instance_key("taggle",gm)
        self.assertEqual(gm["device.keys"][k1], "taggle")
        k2=create_device_instance_key("toggle",gm)
        self.assertEqual(gm["device.keys"][k2], "toggle")
        
    def test_add_rem_two_keys(self):
        gm={}
        k1=create_device_instance_key("taggle",gm)
        self.assertEqual(gm["device.keys"][k1], "taggle")
        k2=create_device_instance_key("toggle", gm)
        self.assertEqual(gm["device.keys"][k2], "toggle")
        remove_device_instance_key(k1,gm,{})
        self.assertTrue(k1 not in gm["device.keys"])
        remove_device_instance_key(k2,gm,{})
        self.assertTrue(k2 not in gm["device.keys"])
            
    
if __name__=="__main__":
    unittest.main()
