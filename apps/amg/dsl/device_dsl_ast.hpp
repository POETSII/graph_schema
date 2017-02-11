#ifndef device_dsl_ast_hpp
#define device_dsl_ast_hpp


#include <string>
#include <map>
#include <memory>
#include <vector>
#include <cassert>

class DataType
{
public:
  virtual ~DataType()
  {}
};
typedef std::shared_ptr<DataType> DataTypePtr;

class DataTypeScalar
  : public DataType
{
private:
    std::string m_name;
    
public:
    DataTypeScalar(std::string name)
        : m_name(name)
    {}
};

class Clause;
typedef std::shared_ptr<Clause> ClausePtr;

class SendClause;
typedef std::shared_ptr<SendClause> SendClausePtr;

class OnRecvClause;
typedef std::shared_ptr<OnRecvClause> OnRecvClausePtr;

class OnRecvDisableClause;
typedef std::shared_ptr<OnRecvDisableClause> OnRecvDisableClausePtr;

class LoopClause;
typedef std::shared_ptr<LoopClause> LoopClausePtr;

class SeqClause;
typedef std::shared_ptr<SeqClause> SeqClausePtr;

class WaitClause;
typedef std::shared_ptr<WaitClause> WaitClausePtr;

class ClauseVisitor
{
public:
    virtual ~ClauseVisitor()
    {}
    
    virtual void visit(SendClausePtr clause) =0;
    virtual void visit(OnRecvClausePtr clause) =0;
    virtual void visit(OnRecvDisableClausePtr clause) =0;
    virtual void visit(WaitClausePtr clause) =0;
    virtual void visit(LoopClausePtr clause) =0;
    virtual void visit(SeqClausePtr clause) =0;
};

class Clause
    : public std::enable_shared_from_this<Clause>
{
public:
    virtual ~Clause()
    {}
    
    virtual void accept(ClauseVisitor *visitor) =0;
};

class SendClause
    : public Clause
{
private:
    std::string m_port;
    std::string m_body;
    
public:
    SendClause(std::string port, std::string body)
        : m_port(port)
        , m_body(body)
    {}

    virtual void accept(ClauseVisitor *visitor)
    {
        visitor->visit(std::static_pointer_cast<SendClause>(shared_from_this()));
    }
};

class OnRecvClause
    : public Clause
{
private:
    std::string m_port;
    std::string m_body;
public:
    OnRecvClause(std::string port, std::string body)
        : m_port(port)
        , m_body(body)
    {}
    
    const std::string &getPort() const
    { return m_port; }
    
    virtual void accept(ClauseVisitor *visitor)
    {
        visitor->visit(std::static_pointer_cast<OnRecvClause>(shared_from_this()));
    }
};

class OnRecvDisableClause
    : public Clause
{
private:
    std::string m_port;
public:
    OnRecvDisableClause(std::string port)
        : m_port(port)
    {}
    
    const std::string &getPort() const
    { return m_port; }
    
    virtual void accept(ClauseVisitor *visitor)
    {
        visitor->visit(std::static_pointer_cast<OnRecvDisableClause>(shared_from_this()));
    }

};

class WaitClause
    : public Clause
{
private:
    std::string m_cond;
public:
    WaitClause(std::string cond)
        : m_cond(cond)
    {}

    virtual void accept(ClauseVisitor *visitor)
    {
        visitor->visit(std::static_pointer_cast<WaitClause>(shared_from_this()));
    }


};

class LoopClause
    : public Clause
{
private:
    ClausePtr m_body;
public:
    LoopClause(ClausePtr body)
        : m_body(body)
    {}
    
    virtual void accept(ClauseVisitor *visitor)
    {
        visitor->visit(std::static_pointer_cast<LoopClause>(shared_from_this()));
    }

    ClausePtr getBody() const
    { return m_body; }
};

class SeqClause
    : public Clause
{
private:
    std::vector<ClausePtr> m_body;
public:
    SeqClause()
    {}
            
    void addClause(ClausePtr clause)
    {
        assert(clause);
        m_body.push_back(clause);
    }
    
    virtual void accept(ClauseVisitor *visitor)
    {
        visitor->visit(std::static_pointer_cast<SeqClause>(shared_from_this()));
    }
    
    const std::vector<ClausePtr> &getClauses() const
    {
        return m_body;
    }

};
typedef std::shared_ptr<SeqClause> SeqClausePtr;


class DataDecl
{
private:
    std::string m_id;
    DataTypePtr m_type;
    std::string m_init;
public:
    DataDecl(std::string id, DataTypePtr type, std::string init="")
        : m_id(id)
        , m_type(type)
        , m_init(init)
    {}
        
    std::string getId() const
    { return m_id; }
};
typedef std::shared_ptr<DataDecl> DataDeclPtr;

inline void addDataDecl(std::vector<DataDeclPtr> &decls, std::string id, DataTypePtr type, std::string init="")
{
    for(auto x : decls){
        if(x->getId()==id)
            throw std::runtime_error("A decl called '"+id+"' already exists.");
    }
    decls.push_back(std::make_shared<DataDecl>(id,type,init));
}

class Pin
{
protected:
    std::string m_name;
    std::string m_messageTypeId;
public:
    Pin(std::string _name, std::string _messageTypeId)
        : m_name(_name)
        , m_messageTypeId(_messageTypeId)
    {}

    virtual ~Pin()
    {}
    
    std::string getName() const
    { return m_name; }
};



class InputPin
    : public Pin
{
private:
    std::vector<DataDeclPtr> m_properties;
    std::vector<DataDeclPtr> m_state;
public:
    InputPin(std::string _name, std::string _messageTypeId)
        : Pin(_name,_messageTypeId)
    {}
    
    void addProperty(std::string _name, DataTypePtr _type, std::string init="")
    {
        addDataDecl(m_properties, _name, _type, init);
    }
    
    void addState(std::string _name, DataTypePtr _type, std::string init="")
    {
        addDataDecl(m_properties, _name, _type, init);
    }
};
typedef std::shared_ptr<InputPin> InputPinPtr;

class OutputPin
    : public Pin
{
private:
    std::string name;
    DataTypePtr type;    
    
public:
    OutputPin(std::string _name, std::string _messageTypeId)
        : Pin(_name,_messageTypeId)
    {}

};
typedef std::shared_ptr<OutputPin> OutputPinPtr;

class DeviceType
{
private:
  std::string m_id;
  std::vector<DataDeclPtr> m_properties;
  std::vector<DataDeclPtr> m_state;
  std::vector<InputPinPtr> m_inputs;
  std::vector<OutputPinPtr> m_outputs;

  SeqClausePtr m_clause;
public:
  DeviceType(std::string id)
    : m_id(id)
    , m_clause(std::make_shared<SeqClause>())
  {}
  
  std::string getId() const
  { return m_id; }
  
  const std::vector<InputPinPtr> &getInputs() const
  { return m_inputs; }
  
  const std::vector<OutputPinPtr> &getOutputs() const
  { return m_outputs; }
  
  void addProperty(std::string name, DataTypePtr type, std::string init="")
  {
    addDataDecl(m_properties, name, type, init);
  }
  
  void addState(std::string name, DataTypePtr type, std::string init="")
  {
    addDataDecl(m_state, name, type, init);      
  }
  
  void addOutput(OutputPinPtr output)
  {
      for(auto x : m_outputs){
          if(x->getName()==output->getName())
              throw std::runtime_error("Output pin '"+output->getName()+"' already exists.");
      }
      m_outputs.push_back(output);
  }
  
  void addInput(InputPinPtr input)
  {
      for(auto x : m_inputs){
          if(x->getName()==input->getName())
              throw std::runtime_error("Input pin '"+input->getName()+"' already exists.");
      }
      m_inputs.push_back(input);
  }
  
  void addClause(ClausePtr clause)
  {
      m_clause->addClause(clause);
  }

  ClausePtr getClause() const
  { return m_clause; }
};
typedef std::shared_ptr<DeviceType> DeviceTypePtr;

class GraphType
{
private:
  std::string m_id;
  std::vector<DeviceTypePtr> m_devices;
public:
  GraphType(std::string id)
    : m_id(id)
  {}
  
  std::string getId() const
  { return m_id; }
  
  void addDeviceType(DeviceTypePtr d)
  {
    for(auto e : m_devices){
      if(d->getId() == e->getId()){
        throw std::runtime_error("Device with id '"+d->getId()+"' already exists.");
      }
    }
    m_devices.push_back(d);
  }
  
  const std::vector<DeviceTypePtr> &getDevices() const
  { return m_devices; }
};
typedef std::shared_ptr<GraphType> GraphTypePtr;

#endif
