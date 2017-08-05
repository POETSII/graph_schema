#include "cpp_op2.hpp"

#include <cmath>

typedef float working_t;

struct Globals
{
    double gam;
    double gm1;
    double cfl;
    double eps;
    double mach;
    double alpha;
    std::array<double,4> qinf;
};

struct Node
{
    std::array<double,2> x;
};

struct Edge
{
    void res_calc(
        const Globals &g,
        const std::array<double,2> &x1,
        const std::array<double,2> &x2,
        const std::array<double,4> &q1,
        const std::array<double,4> &q2,
        double adt1,
        double adt2,
        std::array<double,4> &res1,
        std::array<double,4> &res2 
    ){
        double dx,dy,mu, ri, p1,vol1, p2,vol2, f;

        dx = x1[0] - x2[0];
        dy = x1[1] - x2[1];
        ri   = 1.0/q1[0];
        p1   = g.gm1*(q1[3]-0.5*ri*(q1[1]*q1[1]+q1[2]*q1[2]));
        vol1 =  ri*(q1[1]*dy - q1[2]*dx);
        ri   = 1.0/q2[0];
        p2   = g.gm1*(q2[3]-0.5*ri*(q2[1]*q2[1]+q2[2]*q2[2]));
        vol2 =  ri*(q2[1]*dy - q2[2]*dx);
        mu = 0.5*((adt1)+(adt2))*g.eps;
        f = 0.5*(vol1* q1[0]         + vol2* q2[0]        ) + mu*(q1[0]-q2[0]);
        res1[0] += f;
        res2[0] -= f;
        f = 0.5*(vol1* q1[1] + p1*dy + vol2* q2[1] + p2*dy) + mu*(q1[1]-q2[1]);
        res1[1] += f;
        res2[1] -= f;
        f = 0.5*(vol1* q1[2] - p1*dx + vol2* q2[2] - p2*dx) + mu*(q1[2]-q2[2]);
        res1[2] += f;
        res2[2] -= f;
        f = 0.5*(vol1*(q1[3]+p1)     + vol2*(q2[3]+p2)    ) + mu*(q1[3]-q2[3]);
        res1[3] += f;
        res2[3] -= f;
    }
};

struct BEdge
{
    int bound;
    
    void bres_calc(
        const Globals &g,
        const std::array<double,2> &x1,
        const std::array<double,2> &x2,
        const std::array<double,4> &q1,
        double adt1,
        std::array<double,4> &res1
    ){
        double dx, dy, ri, p1, p2, vol1, vol2, mu, f;
        
        dx = x1[0] - x2[0];
        dy = x1[1] - x2[1];
        ri = 1.0/q1[0];
        p1 = g.gm1*(q1[3]-0.5*ri*(q1[1]*q1[1]+q1[2]*q1[2]));
        if (bound==1){
            res1[1] += + p1*dy;
            res1[2] += - p1*dx;
        }else{
            vol1 =  ri*(q1[1]*dy - q1[2]*dx);
            ri   = 1.0/g.qinf[0];
            p2   = g.gm1*(g.qinf[3]-0.5*ri*(g.qinf[1]*g.qinf[1]+g.qinf[2]*g.qinf[2]));
            vol2 =  ri*(g.qinf[1]*dy - g.qinf[2]*dx);
            mu = (adt1)*g.eps;
            f = 0.5*(vol1* q1[0]         + vol2* g.qinf[0]        ) + mu*(q1[0]-g.qinf[0]);
            res1[0] += f;
            f = 0.5*(vol1* q1[1] + p1*dy + vol2* g.qinf[1] + p2*dy) + mu*(q1[1]-g.qinf[1]);
            res1[1] += f;
            f = 0.5*(vol1* q1[2] - p1*dx + vol2* g.qinf[2] - p2*dx) + mu*(q1[2]-g.qinf[2]);
            res1[2] += f;
            f = 0.5*(vol1*(q1[3]+p1)     + vol2*(g.qinf[3]+p2)    ) + mu*(q1[3]-g.qinf[3]);
            res1[3] += f;
        }
    }
};

struct Cell
{
    std::array<double,4> q;
    std::array<double,4> qold;
    double adt;
    std::array<double,4> res;
    
    void save_soln(
        const Globals &g
    ){
        qold=q;
    };
    
    void adt_calc(
        const Globals &g,
        const std::array<double,2> &x1,
        const std::array<double,2> &x2,
        const std::array<double,2> &x3,
        const std::array<double,2> &x4
    ){
        double dx,dy, ri,u,v,c;
        
        adt=0.0;
        
        ri =  1.0/q[0];
        u  =   ri*q[1];
        v  =   ri*q[2];
        c  = sqrt(g.gam*g.gm1*(ri*q[3]-0.5*(u*u+v*v)));
        dx = x2[0] - x1[0];
        dy = x2[1] - x1[1];
        adt += fabs(u*dy-v*dx) + c*sqrt(dx*dx+dy*dy);
        dx = x3[0] - x2[0];
        dy = x3[1] - x2[1];
        adt += fabs(u*dy-v*dx) + c*sqrt(dx*dx+dy*dy);
        dx = x4[0] - x3[0];
        dy = x4[1] - x3[1];
        adt += fabs(u*dy-v*dx) + c*sqrt(dx*dx+dy*dy);
        dx = x1[0] - x4[0];
        dy = x1[1] - x4[1];
        adt += fabs(u*dy-v*dx) + c*sqrt(dx*dx+dy*dy);
        adt = adt / g.cfl;
    }
    
    void update(
        const Globals &g,
        double &rms
    ){
        double adti = 1.0/adt;
        for(unsigned i=0; i<4; i++){
            double ddel    = adti*res[i];
            q[i]   = qold[i] - ddel;
            res[i] = 0.0;
            rms  += ddel*ddel;
        }
    }
};




int main(int argc, char *argv[])
{
    const char *srcFile=argv[1];
    
    H5::H5File file(srcFile, H5F_ACC_RDONLY);
    
    Globals globals;
    read_global(file,"gam",globals.gam);
    read_global(file,"gm1",globals.gm1);
    read_global(file,"cfl",globals.cfl);
    read_global(file,"eps",globals.eps);
    read_global(file,"mach",globals.mach);
    read_global(file,"alpha",globals.alpha);
    read_global(file,"qinf",globals.qinf);
    
    auto cells=read_set<Cell>(file, "cells");
    auto nodes=read_set<Node>(file, "nodes");
    auto edges=read_set<Edge>(file, "edges");
    auto bedges=read_set<BEdge>(file, "bedges");
    
    auto pedge=read_map<2>(file, "pedge", edges, nodes);
    auto pecell=read_map<2>(file, "pecell", edges, cells);
    auto pbedge=read_map<2>(file, "pbedge", bedges, nodes);
    auto pbecell=read_map<1>(file, "pbecell", bedges, cells);
    auto pcell=read_map<4>(file, "pcell", cells, nodes);
    
    read_dat<Node,double,2>(file,"p_x",nodes,&Node::x);
    read_dat<Cell,double,4>(file,"p_q",cells,&Cell::q);
    read_dat<Cell,double,4>(file,"p_q",cells,&Cell::qold);
    read_dat<Cell,double,4>(file,"p_res",cells,&Cell::res);
    read_dat<Cell,double,1>(file,"p_adt",cells,&Cell::adt);
    read_dat<BEdge,int,1>(file,"p_bound",bedges,&BEdge::bound);
    
    fprintf(stderr, "Running\n");
    int niter = 1000;
    for(int iter=1; iter <= niter; iter++)
    {
        fprintf(stderr, "  Iter %d\n", iter);

        for(unsigned ci=0; ci<cells.size(); ci++){
            cells[ci].save_soln(globals);
        }
        
        double rms;
        
        for(int k=0;k<2;k++){
            for(unsigned ci=0; ci<cells.size(); ci++){
                cells[ci].adt_calc(globals,
                    nodes[pcell[ci][0]].x,// read
                    nodes[pcell[ci][1]].x,// read
                    nodes[pcell[ci][2]].x,// read
                    nodes[pcell[ci][3]].x// read                
                );
            }
            
            for(unsigned ei=0; ei<edges.size(); ei++)
            {
                edges[ei].res_calc(globals,
                    nodes[pedge[ei][0]].x,// read
                    nodes[pedge[ei][1]].x,// read
                    cells[pecell[ei][0]].q,// read
                    cells[pecell[ei][1]].q,// read
                    cells[pecell[ei][0]].adt,// read
                    cells[pecell[ei][1]].adt, // read
                    cells[pecell[ei][0]].res, // inc
                    cells[pecell[ei][1]].res // inc
                );
            }
            
            for(unsigned bei=0; bei<bedges.size(); bei++)
            {
                bedges[bei].bres_calc(globals,
                    nodes[pbedge[bei][0]].x,
                    nodes[pbedge[bei][1]].x,
                    cells[pbecell[bei][0]].q,
                    cells[pbecell[bei][0]].adt,
                    cells[pbecell[bei][0]].res
                );
            }
            
            /*
            parallel_for(
                BEdge::bres_calc,
                globals,
                read(pbedge, &BEdge::x, _0),
                read(pbedge, &BEdge::x, _1),
                write(pbecell, &Cell::q),
                write(pbecell, &Cell::adt),
                write(pbecell, 0&Cell::res)
            );
            */
            
            rms=0.0;
            for(unsigned ci=0; ci<cells.size(); ci++){
                cells[ci].update(globals,
                    rms
                );
            }
        }
        
        /*
        parallel_for(
            Cells::update,
            globals,
            inc(rms)
        )
        */
        
        rms = sqrt(rms / cells.size());
        if((iter%100)==0){
            fprintf(stdout, " %d  %10.5e\n", iter, rms);
        }
    }
}
