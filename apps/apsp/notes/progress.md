We have:

    s_1,...,s_n : state of our n nodes.
    F(s_i) = Set of node addresses that this node sents to
    rts(s_i) = Node i wants to send
    ts(s_i) = Number of messages sent by s_i
    tr(s_i) = Number of messages received by s_i
    rts(s_i) = Count of messages still to be sent by node s_i (simple model)
    
    M : Multi-set of node addresses.
        Represents messages still to be delivered.
    
    P : Multi-set of (sent,recv) pairs representing progress messages
        

State transitions are:

    i \in M -> {   // Receive at node i
        tr(s_i) <- tr(s_i) + 1
        M <- M - {i}
    }
    
    rts(s_i) > 0 -> {  // Send of node i
        ts(s_i) += |F(s_i)|
        rts(s_i) = rts(s_i) - 1
        M <- M \cup F(s_i)
    }
    
    rts(s_i) ==0 -> {  // Send progress for s_i
        P <- P \cup (ts(s_i),tr(s_i))
    }
    
    (sent,recv) \in P {
        totalSent <- totalSent+sent
        totalRecv <- totalRecv+recv
        P <- P - {(sent,recv)}
    }
    
Requirements:

    totalSent==totalRecv -> M == \empty
