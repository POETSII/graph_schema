
class Expr:
    def __init__(self):
        pass
        
    def __getitem__(self,property):
        return ReadIndexExpr(self,property)
                
    def __getattr__(self,index):
        return ReadElementExpr(self,index)

    def set(self, src):
        return AssignStat(self,src)


class AddExpr(Expr):
    def __init__(self,a,b):
        self.a=a
        self.b=b
        
    def render_c(self):
        return "({} + {})".format(self.a.render_c(), self.b.render_c())

class Stat:
    def __init__(self):
        pass

class DataExpr(Expr):
    def __init__(self,name):
        self.name=name
        
    def render_c(self):
        return self.name
        
class ReadElementExpr(Expr):
    def __init__(self,src,member):
        self.src=src
        self.property=property

    def render_c(self):
        return "({}.{})".format(self.src.render_c(),self.property)

class ReadIndexExpr(Expr):
    def __init__(self,src,index):
        self.src=src
        self.index=index
        
    def "({}[{}])".format(self.src.render_c(),self.index.render_c())

class InputPort:
    def __init__(self,name,on_receive):
        self.name=name
        self.on_receive=on_receive
        
    def render_c(self):
        
        
def input(name,on_receive,):
    return InputPort(name,on_receive)

class OutputPort:
    def __init__(self,name,on_send,ready_to_send):
        self.name=name
        self.on_receive=on_receive
        self.ready_to_send=ready_to_send

def output(name,on_receive,):
    return InputPort(name,on_receive)


class DeviceType:
    def __init__(self,name,ports):
        self.name=name
        self.ports=ports
        
    def render_c(self):
        for p  in self.ports:
            p.render_c()
        
def device_type(name,ports):
    return DeviceType(name,ports)


flat = device_type("flat", [
    input("input",
        lambda (gp,dp,ds,ep,es,m):
        [
            IfElse( ds.t==m.t, [
                ds.cSeen.set( ds.cSeen + 1 ),
                ds.cAcc.set( ep.w * m.v )
            ],[
                ds.nSeen.set( ds.nSeen + 1 ),
                ds.nAcc.set( ds.nAcc + ep.e * m.v )
            ])
        ]
    ),
    output("output",
        lambda (gp,dp,ds,m):
        [
            m.t.set( ds.t ),
            m.v.set( ds.v )
        ]
    )
])

