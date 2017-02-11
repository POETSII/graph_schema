
struct flat_cell
{  
    struct properties_t
    {
        unsigned nhoodSize;     // Number of neighbours
        fixed_t wSelf; // Contribution of current value to next step
        unsigned dt;
    };

    struct state_t
    {
        unsigned t;
        unsigned cSeen;
        fixed_t cAcc;
        unsigned nSeen;
        fixed_t nAcc;
    };
    
    struct input{
        typedef update_message_t message_t;
        
        struct edge_properties_t{
            fixed_t w;
        };
        
        struct edge_state_t {};
        
        static void on_receive(
            const graph_properties_t *graphProperties,
            const properties_t *deviceProperties,
            state_t *deviceState,
            const edge_properties_t *edgeProperties,
            edge_state_t */*edgeState*/,
            const update_message_t *message
        ){
            if(message->t == deviceState->t){
                deviceState->cSeen++;
                deviceState->cAcc += fixed_mul(edgeProperties->w, message->v);
            }else if(message->t == deviceState->t + deviceProperties->dt){
                deviceState->nSeen++;
                deviceState->nAcc += fixed_mul(edgeProperties->w, message->v);                
            }else{
                assert(0);
            }
        }
    };
    
    struct output{  
        typedef update_message_t message_t;
        
        static bool ready_to_send(
            const graph_properties_t *graphProperties,
            const properties_t *deviceProperties,
            const state_t *deviceState
        ){
            return deviceProperties->nhoodSize == deviceState->cSeen;
        }
        
        static bool on_send(
            const graph_properties_t *graphProperties,
            const properties_t *deviceProperties,
            state_t *deviceState,
            update_message_t *message
        ){
            assert(ready_to_send(graphProperties,deviceProperties,deviceState);
            
            message->t = deviceState->t;
            message->v = deviceState->cAcc;
            
            deviceState->t += deviceProperties->dt;
            deviceState->cSeen = deviceState->nAcc; 
            deviceState->cAcc  = deviceState->nAcc + fixed_mul(deviceState->cAcc, deviceProperties->wSelf);
            deviceState->nSeen = 0;
            deviceState->nAcc = 0;
            
            return true;
        }
    };
    
    typedef input_port_list<
        input
    > inputs;
    
    typedef ouput_port_list<
        output
    > inputs;
};
