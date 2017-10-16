

class Cell
{
    float df2x2_mid(float L, float M, float R)
    {
        
    }
    
    int t[4];       /* Times for L,R,U,D
                2
              0   1
                3
        
    float u[8];    /*
                0
                1
            2 3 4 5 6
                7
                8
    */
    
    void step()
    {
        float dx2;
        if(times[0]==t && times[1]==t){
            dx2=df2dx2_mid(u[3],u[4],u[5]);
        }else if(times[0]==t){
            dx2=df2dx2_left(u[2],u[3],u[4]);
        }else{
            assert(times[1]==t);
            dx2=df2dx2_right(u[4],u[5],u[6]);
        }
        float dy2;
        if(times[2]==t && times[3]==t){
            dy2=df2dy2_mid(u[1],u[4],u[7]);
        }else if(times[2]==t){
            dy2=df2dy2_up(u[0],u[1],u[4]);
        }else{
            assert(times[3]==t);
            dy2=df2dy2_down(u[4],u[7],u[8]);
        }
    }
};
