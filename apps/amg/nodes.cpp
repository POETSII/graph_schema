

class Node
{
    
};

class WeightMap
{
private:
    std::vector< std::pair<uint32_t,float> > m_w;
public:
    
    float operator[](uint32_t x) const
    {
        for(unsigned i=0; i<m_w.size(); i++){
            if(m_w[i].first==x)
                return m_w[i].second;
        }
        assert(0);
        throw std::runtime_error("Invalid index.");
    }
};

struct peer_msg_t
{
    uint32_t src;
    float x;
};

struct fine_to_coarse_msg_t
{
    uint32_t src;
    float x;        // Current guess (may well be zero, but on lowest level is not)
    float b;        // Value to solve for
    float max_r;    // Max residual of all finer nodes (though not for whole network)
};

struct coarse_to_fine_msg_t
{
    uint32_t src;
    float x;        // Current guess
    float max_r;    // Max residual of all nodes on the finest layer across the whole network
};

class MidNode
{
    enum{
        CollectingFine,
        PreSmooth,
        CollectingCoarse,
        PostSmooth
    };
    
    
/*
device mid[
    
    const 
    
    int   seenPeer=0;
    float accPeer=0;
    int   seenCoarse=0;
    float accCoarse=0;
    int   seenFine=0;
    float accFine=0;
    float maxResidual=0;
    
    input fine_to_coarse fine_up {
        const float R; // restriction weight
    };
    
    output coarse_to_fine fine_down;
    
    output coarse_to_fine coarse_up;
    
    input fine_to_coarse coarse_down {
        const float P; // prolongation weight
    };

    input exchange peer_in {
        const float A; // Matrix to solve
    };
    
    output exhange peer_out;

    loop[
        recv/fine_in[
            assert( s.fineSeen < p.fineTotal );
            s.fineAccX += ep.R * msg.x;
            s.fineAccB += ep.R * msg.b;
            s.maxResidual = max(s.maxResidual, msg.max_r);
            s.fineSeen++;
        ]
    
        wait( s.fineSeen == p.fineTotal )
    
        recv/fine_in/disable // We can't receive another fine message till we have sent more info to fine 
    
        send/peer_out[
            s.x = s.fineAccX; // We now have the full value for x on the way up (likely to be zero on most levels)
            s.b = s.fineAccB; // ... and for b
            s.fineAccX=0;
            s.fineAccB=0;
            s.fineSeen=0;
        
            // Broadcast our value for jacobi iteration
            msg.x = x;
        ]
        
        recv/peer_in[
            assert( s.peerSeen < p.peerTotal );
            s.peerAcc += edge.A * msg.x;
            s.peerSeen++;
        ]
    
        wait(s.peerSeen == p.peerTotal)
        
        recv/peer_in/disable // We can't receive another peer message till we have sent info upwards 
        
        send/coarse_up[
            // Got all values for jacobi, so can move forwards
            // peerAcc == Ar * x,  A=Ad+Ar
            
            // _unsmoothed_ residual. This only works well if the restriction
            // is doing some kind of smoothing, i.e. the prolongation is some
            // kind of linear interpolation matrix. Empirically it seems to work fine...
            float r = s.b - (s.peerAcc - p.Ad * 
            
            // One jacobi iteration, as we might as well still smooth, even if the
            // residual doesn't contain it
            s.x += ( s.b - s.peerAcc ) * p.AdiagInvOmega - p.omega * s.x

            countPeer=0;
            accPeer=0;
            
            msg.x=0;
            msg.b=r; // Note that we are sending the unsmoothed version here
            msg.max_r=s.max_r;
        ]
        
        recv/coarse_down[
            assert( s.coarseSeen < s.totalCoarse );
            s.coarseAcc += edge.P * msg.x;
            s.maxResidual = max(s.maxResidual, msg.max_r);
            s.coarseSeen++;
        ]
        
        // We could receive post-smooth messages from peers before
        // we get all our coarse messages. The same message handler
        // still works
        recv/peer_in/enable
        
        wait(s.coarseSeen==p.coarseCount)
        
        recv/coarse_in/disable;
    
        send/peer_out[
            s.x += s.coarseAcc;
        
            s.coarseAcc=0;
            s.coarseSeen=0;

            msg.x=s.x;
        ]
                
        wait(s.peerSeen == p.peerCount )
        
        recv/peer_in/disable;
        
        send/fine_down[
            // We don't need to calculate residual, as we only care about
            // max residual accross whole mesh
            // Same as for previous relaxation
            s.x += ( s.b - s.peerAcc ) * p.AdiagInvOmega - p.omega * s.x

            msg.x = s.x;
            msg.max_r = s.maxResidual;
            
            // Reset for next round
            s.fineAccX = 0;
            s.fineAccB = 0;
            s.maxResidual = 0;
            s.fineSeen = 0;
            s.peerSeen=0;
            s.peerAcc=0;
        ]
    
        recv/fine_up/enable // Can only get the next fine after we have sent ours
    ]
    */
    
    uint32_t m_id;
    
    WeightMap m_A;
    WeightMap m_P;
    WeightMap m_R;
    
    float peerAcc;
    int peerSeen;
    
    
    // Information coming from finer mesh
    void on_recv_fine(
        const fine_to_coarse_msg_t *msg 
    ){        
        fineAcc += msg->x * m_R[msg->src];
        fineSeen ++;
    }
    
    void on_send_peer(
        peer_msg_t *msg
    ){
        // Either we just got new information from fine, or we got
        // a correction from coarse
        assert(m_state==CollectFine || m_state==CollectCoarse);
        
        float acc;
        if(m_state==CollectFine){
            acc=m_accFine;
            m_accFine=0;
            m_seenFine=0;
            m_state=CollectPreSmooth;
            
            m_x= (m_b - acc) * m_AdiagInvOmega - m_omega*m_x;
        
            msg->x = m_x;
        }else{
            acc=m_accCoarse;
            m_accCoarse=0;
            m_seenCoarse=0;
            m_state=CollectPostSmooth;
        }
        
    }
    
    void on_recv_peer(
        const peer_msg_t *msg
    ){
        m_peerAcc += msg->x * m_A[msg->src];
        m_peerSeen ++;
    };
    
    void on_send_coarse(
        const fine_to_coarse_msg_t *msg
    ){
        assert(m_state==CollectPreSmooth)

            update x based on acc from peers
            calculate r
    
            send r
    
            countPeer=0
            state=CollectCoarse
    }
    
    
    
    // Information coming from coarser mesh
    void on_recv_bwd(
    
    ){
        coarseAcc += x * m_P[srcId];
        coarseSeen ++;        
    }
};
