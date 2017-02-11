#include "device_dsl_ast.hpp"

#include <cassert>

/* A state is a segment of one or more primitive
    non-blocking clauses (on_recv) terminated
    by one or more primitive blocking clauses (wait,send).
    
    NOTE: Currently there can only be one terminator
    
    Compositive clauses (seq, loop) must be broken
    down, and do not appear in states.
    
    Each state also has a set of ambient on_recv
    clauses for each port, which may be empty at
    any point in time.
*/
class State
{
public:
    unsigned stateId;
    
    State(unsigned _stateId)
        : stateId(_stateId)
        , nextStateId(-1)
    {}
    
    
    // Map of portName->clause
    std::map<std::string,OnRecvClausePtr> ambient;
    
    std::vector<ClausePtr> clauses; // Clauses in this state. Last one must be terminator
    unsigned nextStateId;
};
typedef std::shared_ptr<State> StatePtr;

class EnumStatesVisitor
    : public ClauseVisitor
{
public:
    
    std::vector<StatePtr> states;
    unsigned currStateId = -1;
    unsigned prevStateId = -1;
    
    EnumStatesVisitor(DeviceTypePtr device)
    {
        
        device->getClause()->accept(this);
    }
    
    StatePtr getActive()
    {
        if(currStateId==-1){
            currStateId=states.size();
            states.push_back(std::make_shared<State>(currStateId));
            if(prevStateId!=-1){
                states.at(prevStateId)->nextStateId=currStateId;
            }
            prevStateId=-1;
        }
        return states[currStateId];
    }
    
    void finishActive()
    {
        if(currStateId==-1){
            throw std::runtime_error("No state to finish.");
        }
        prevStateId=currStateId;
        currStateId=-1;
    }
    
    virtual void visit(OnRecvDisableClausePtr clause)
    {   
        getActive()->ambient.at(clause->getPort())=nullptr;
        getActive()->clauses.push_back(clause);
    }
    
    virtual void visit(OnRecvClausePtr clause)
    {   
        getActive()->ambient.at(clause->getPort())=clause;
        getActive()->clauses.push_back(clause);
    }
    
    virtual void visit(SendClausePtr clause)
    {
        getActive()->clauses.push_back(clause);
        finishActive();
    }
    
    virtual void visit(WaitClausePtr clause)
    {
        getActive()->clauses.push_back(clause);
        finishActive();
    }
    
    virtual void visit(SeqClausePtr clause)
    {
        bool hitLoop=false;
        for(ClausePtr p : clause->getClauses()){
            if(hitLoop){
                throw std::runtime_error("Loops currently never exit, but something is following a loop.");
            }
            p->accept(this);
            if(std::dynamic_pointer_cast<LoopClause>(p)){
                hitLoop=true;
            }
        }
    }
    
    virtual void visit(LoopClausePtr clause)
    {
        // This will be updated by the loop body with all the
        // ambient
        StatePtr enter=getActive();
        
        // Number of things that happened before the loop body
        int numPreEnterActions=enter->clauses.size();
            
        // This call will:
        // - possibly add non-blocking clauses
        // - definitely add a blocking clause (otherwise we have combinational loop)
        clause->getBody()->accept(this);
        
        StatePtr exit=getActive();
        if(enter==exit){
            throw std::runtime_error("Loop seems to have executed without a blocking clause.");
        }
                
        // The loop exit is now updated with everything from the beginning of
        // the loop to the clause.
        for(int i=numPreEnterActions; i<enter->clauses.size(); i++){
            enter->clauses[i]->accept(this);
        }
        
        // There should now be no active state
        assert(currStateId==-1);
    }
};

void enum_states(DeviceTypePtr device)
{
    EnumStatesVisitor v(device);
}
