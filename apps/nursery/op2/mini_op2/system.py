from mini_op2.core import *

class SystemSpecification(object):
    def __init__(self):
        self._ids=set()
        self.globals={}
        self.sets={}
        self.dats={}
        self.maps={}
        
    def add_global(self, global_:Global):
       assert global_.id not in self._ids
       self._ids.add(global_.id)
       self.globals[global_.id]=global_
       
    def create_const_global(self, *args, **kwargs) -> ConstGlobal:
        g=ConstGlobal(*args,**kwargs)
        self.add_global(g)
        return g
        
    def create_mutable_global(self, *args, **kwargs) -> MutableGlobal:
        g=MutableGlobal(*args,**kwargs)
        self.add_global(g)
        return g

    def add_set(self, set:Set):
        assert set.id not in self._ids
        self._ids.add(set.id)
        self.sets[set.id]=set
    
    def create_set(self, *args, **kwargs) -> Set:
        g=Set(*args,**kwargs)
        self.add_set(g)
        return g
        
    def add_dat(self, dat:Dat):
        assert dat.set.id in self.sets
        assert dat.id not in self._ids
        self._ids.add(dat.id)
        self.dats[dat.id]=dat
        
    def create_dat(self, *args, **kwargs) -> Dat:
        g=Dat(*args,**kwargs)
        self.add_dat(g)
        return g
        
    def add_map(self, map:Map):
        assert map.iter_set.id in self.sets
        assert map.to_set.id in self.sets
        assert map.id not in self._ids
        self._ids.add(map.id)
        self.maps[map.id]=map
        
    def create_map(self, *args, **kwargs) -> Map:
        g=Map(*args,**kwargs)
        self.add_map(g)
        return g

class SystemInstance(object):
    def __init__(self, spec:SystemSpecification, src:Dict[str,Any]) -> None:
        """Given a system instance, load the state from hdf5 (well, a dictionary)
        
        Size of sets is given by scalar int with the same name, and must exist.
        
        Globals must match the data_type, and will be zero initialised if they don't exist.
        
        Maps are given by integer arrays of shape (len(set),arity), and must exist.
        
        Dats are optionally given by arrays of (len(set),*). If they don't exist, they will be zero initialised.
        """
        self.spec=spec
        self.globals={} # type:Dict[Global,numpy.ndarray]
        self.sets={}    # type:Dict[Set,int]
        self.dats={}    # type:Dict[Dat,numpy.ndarray]
        self.maps={}    # type:Dict[Map,numpy.ndarray]
        
        for g in spec.globals.values():
            if g.id in src:
                logging.info("Loading global %s", g.id)
                value=g.data_type.import_value(src[g.id])
            else:
                if isinstance(g,ConstGlobal):
                    raise RuntimeError("No value in dictionary for global const {}".format(g.id))
                logging.info("Zero initialising constant %s", g.id)
                value=g.data_type.create_default_value()
            self.globals[g]=value
        
        for s in spec.sets.values():
            if not s.id in src:
                raise RuntimeError("No value in dictionary called '{}' for set size.".format(s.id))
            raw=src[s.id]
            if isinstance(raw,int):
                ival=raw # type:int
            else:
                aval = numpy.array(raw) # type:numpy.ndarray
                ival=int(aval) # type:ignore
            assert ival>=0
            logging.info("Loaded set size of %s = %d", s.id, ival)
            self.sets[s]=ival
        
        for d in spec.dats.values():
            set_size=self.sets[d.set]
            dat_type=DataType( dtype=d.data_type.dtype, shape=(set_size,)+d.data_type.shape)
            if not d.id in src:
                logging.info("Zero initialising dat %s on set %s",d.id,d.set.id) 
                aval=dat_type.create_default_value()
            else:
                logging.info("Loading dat %s on set %s of %u x shape=%s", d.id,d.set.id, set_size, d.data_type.shape)
                arr=src[d.id]
                aval=dat_type.import_value(arr)
            self.dats[d]=aval
        
        for m in spec.maps.values():
            if not s.id in src:
                raise RuntimeError("No value in dictionary called '{}' for map.".format(m.id))
            iter_size=self.sets[m.iter_set]
            to_size=self.sets[m.to_set]
            logging.info("Loading map %s : %s -> %s of size %d x %d", m.id, m.iter_set.id, m.iter_set.id, iter_size, m.arity)
            map_type=DataType( shape=(iter_size,m.arity), dtype=numpy.uint32 )
            raw=src[m.id]
            logging.debug("Value shape=(%s), size=%d", raw.shape, raw.size)
            aval=map_type.import_value(raw)
            if not (aval < to_size).all():
                raise RuntimeError("Mapping points out of bounds in to_set.")
            self.maps[m]=aval
        

def load_hdf5_instance(spec:SystemSpecification, src:str) -> SystemInstance:
    with h5py.File(src) as f:
        return SystemInstance(spec, f)
