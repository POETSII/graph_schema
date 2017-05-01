

struct progress
{
    unsigned version;
    unsigned sent;
    unsigned received;
};

class merger
{
    progress left, right, self;

    on_recv_left(progress &p){
        if(p.version < left.version){
            return;
        }
        assert(p.version != left.version);
        
        left=p;
        dirty=true;
    }
    
    on_recv_right(progress &p){
        if(p.version < right.version){
            return;
        }
        assert(p.version != right.version);
        
        right=p;
        dirty=true;
    }
    
    rts()
    {
        return dirty && left.version>0 && right.version>0;
    }
    
    on_send(progress &p)
    {
        return dirty && left.version>0 && right.version>0;
        
        self.stable=true;
        self.version++;
        self.version.sent = left.version.sent + right.version.sent;
        self.version.recv = left.version.recv + right.version.recv;
        dirty=false;
        
        p=self;
    }
};


N0 N1 N2 N2
  P     P
     P

N0->N2

(1,1,0)  (0,0,0)  (0,0,0)  (0,0,0)
     (0,0,0)           (0,0,0)
              (0,0,0)

N2->N1

(1,1,0)  (0,0,0)  (1,1,0)  (0,0,0)
     (0,0,0)           (0,0,0)
              (0,0,0)
