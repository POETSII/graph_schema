#ifndef state_machine_hpp
#define state_machine_hpp

double g_start;
double g_finish;

double now();

struct sm
{
    unsigned thread=0;
    unsigned state=0;
    uint32_t data[4];
    unsigned elapsed_timestamp;

    bool operator()(uint32_t x)
    {

        if(state==-1){
            fprintf(stderr, "Received word after thread %d finished.\n", thread);
		exit(1);
        }
        if(x&0x80000000ul){
		g_finish=now();
        	elapsed_timestamp=x&0x7FFFFFFFul;
		if(elapsed_timestamp==0){
			fprintf(stderr, "Start\n");
			return true;
                }
	        fprintf(stderr, "Finished : %g, cycles=%u\n", g_finish, elapsed_timestamp);
	       state=-1;	
        	return false;
	}
        if(x==-2){
            g_start=now();
	    fprintf(stderr, "Start : %g\n", g_start);
            state=-2;
            return true;
        }
        assert(state<4);
        data[state]=x;
        state++;
        
        if(state==4){
            unsigned steps=data[0];
            unsigned id=data[1]>>16;
            unsigned colour=data[1]&0xFFFF;
            double xpos=ldexp(int32_t(data[2]),-16);
            double ypos=ldexp(int32_t(data[3]),-16);
            fprintf(stdout, "%u, %d, %f, %d, %d, %f, %f, 0, 0, %d\n",
		thread, steps, steps/64.0, id, colour, xpos, ypos, thread);
            state=0;
        }
        return true;
    }
};

#endif
