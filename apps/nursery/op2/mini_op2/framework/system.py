from mini_op2.framework.core import *

class SystemSpecification(object):
    def __init__(self):
        self._ids=set()
        self.globals={}
        self.sets={}
        self.dats={}
        self.maps={}
        
        # This is an implicit variable used to capture boolean outputs
        # of expressions
        self.create_mutable_global("_cond_", DataType(shape=(1,),dtype=numpy.uint32))
        
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
        sizeof="sizeof_"+set.id
        assert sizeof not in self._ids
        self._ids.add(set.id)
        self.sets[set.id]=set
        self.create_const_global(sizeof, DataType(shape=(1,),dtype=numpy.uint32))
    
    def create_set(self, *args, **kwargs) -> Set:
        g=Set(*args,**kwargs)
        self.add_set(g)
        return g
        
    def get_sizeof_set(self, set:Set) -> ConstGlobal :
        assert set.id in self.sets
        assert self.sets[set.id] is set
        return self.globals["sizeof_"+set.id]
        
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
    def check_snapshot_if_present(self, spec:SystemSpecification, src:Dict[str,Any], snapshot:str) -> None:
        """If a group called {snapshot} exists in the dictionary, then check the current dat
        values against the group"""
        parts=snapshot.split("/")
        while len(parts)>0:
            if len(parts[0])!=0:
                if not parts[0] in src:
                    return # Couldn't find it
                src=src[parts[0]]
            parts=parts[1:]
            
        logging.info("Found reference pattern {}".format(snapshot))
        
        for d in self.spec.dats.values():
            assert d.id in src
            ref=src[d.id]
            got=self.dats[d]
            assert ref.shape==got.shape
            diff=ref-got
            maxErr=numpy.max(numpy.abs(diff))
            if maxErr <=1e-6:
                logging.info("%s : %s, maxErr=%g", snapshot, d.id, maxErr)
            else:
                logging.error("%s : %s, maxErr=%g (Too high)", snapshot, d.id, maxErr)
                logging.error("%s : %s, got=%s, ref=%s", snapshot, d.id, got, ref)
        
        for g in self.spec.globals.values():
            if not g.id in src:
                continue # Not all globals will exist in reference
            try:
                ref=src[g.id]
                got=self.globals[g]
                assert ref.shape==got.shape
                diff=ref-got
                maxErr=numpy.max(numpy.abs(diff))
                if maxErr <= 1e-6 :
                    logging.info("%s : %s, maxErr=%g", snapshot, g.id, maxErr)
                else:
                    logging.error("%s : %s, maxErr=%g (Too high)", snapshot, g.id, maxErr)
                    logging.error("%s : %s, got=%s, ref=%s", snapshot, g.id, got, ref)
            except Exception as e:
                logging.error("Exception while checking item %s in snapshot %s : %s", g.id, snapshot, e)
                raise
        
    
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
        self.checkpoints={} # type:Dict[str,any]
        
        for g in spec.globals.values():
            if g.id.startswith("sizeof_"):
                logging.debug("Skipping implied constant %s", g.id)
                continue
            elif g.id in src:
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
            logging.info("Adding implicit sizeof 'sizeof_%s' = %u", s.id, ival)
            sg=spec.globals["sizeof_{}".format(s.id)]
            self.globals[sg]=sg.data_type.import_value(numpy.array( [ival] ))
        
        for d in spec.dats.values():
            set_size=self.sets[d.set]
            dat_type=DataType( dtype=d.data_type.dtype, shape=(set_size,)+d.data_type.shape)
            try:
                if not d.id in src:
                    logging.info("Zero initialising dat %s on set %s",d.id,d.set.id) 
                    aval=dat_type.create_default_value()
                else:
                    logging.info("Loading dat %s on set %s of %u x shape=%s", d.id,d.set.id, set_size, d.data_type.shape)
                    arr=src[d.id]
                    aval=dat_type.import_value(arr)
            except Exception as e:
                logging.error("Exception while importing dat %s : %s", d.id, e)
                raise
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
        
        if "output" not in src:
            logging.info("No checkpoint data present.")
            for k in src:
                logging.info(k)
        else:
            for (key,cp) in src["output"].items():
                logging.info("Checkpoint %s", key)
                self.checkpoints[key]=cp

def load_hdf5_instance(spec:SystemSpecification, src:str) -> SystemInstance:
    with h5py.File(src) as f:
        return SystemInstance(spec, f)
