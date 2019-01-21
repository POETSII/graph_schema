#include "particle.hpp"

real_t potential_a(real_t dist_squared)
{
    const real_t two=real_t::from_double(2.0);
    const real_t quarter=real_t::from_double(0.25);
    const real_t half=real_t::from_double(0.5);
    

    dist_squared=std::max(dist_squared, quarter);
            
    real_t inv_dist_squared=inv(dist_squared);

    return inv_dist_squared*two - half*inv_dist_squared*inv_dist_squared;
}

real_t potential(real_t dist_squared, int attract)
{
    const real_t neg_quarter=real_t::from_double(-0.25);
    const real_t half=real_t::from_double(0.5);
    //const real_t two=real_t::from_double(2.0);
    
    const real_t thresh=real_t::from_double(0.46);
    dist_squared=std::max(dist_squared, thresh);
            
    real_t inv_dist_pow2=inv(dist_squared);
    real_t inv_dist_pow4=inv_dist_pow2*inv_dist_pow2;
    real_t inv_dist_pow6=inv_dist_pow4*inv_dist_pow2;
    real_t inv_dist_pow8=inv_dist_pow4*inv_dist_pow4;

    if(!attract){
        return neg_quarter * inv_dist_pow8;
    }

    return inv_dist_pow6 - half*inv_dist_pow8;
}


// Force experience by a due to b
vector_t force(const world_info_t &info, const vector_t &a, const vector_t &b, int attract)
{
    vector_t dpos=b-a;
    
    vector_t f; // zero by default
    
    // Avoid overflow in squaring
    if( abs(dpos.x) <= info.horizon && abs(dpos.y) <= info.horizon ){
        real_t dist_squared = dpos.x*dpos.x + dpos.y*dpos.y;
        if( dist_squared <= info.horizon_squared ){
            auto scale=potential(dist_squared, attract);

            
            f.x = dpos.x * scale;
            f.y = dpos.y * scale;

            //std::cerr<<"  F="<<f<<"\n";
        }else{
            //std::cerr<<"Cull2\n";
        }
    }else{
        //std::cerr<<"Cull1\n";
    }
    
    return f;
}

void calculate_particle_intra_forces(
    const world_info_t &info,
    unsigned n,
    particle_t *particles,
    vector_t *forces
){
    for(unsigned i=0; i<n; i++){
        forces[i]=vector_t::zero();
    }
    
    for(unsigned i=0; i+1<n; i++){
        const auto &pi=particles[i];
        for(unsigned j=i+1; j<n; j++){
            const auto &pj=particles[j];
            vector_t f=force(info, pi.position, pj.position, pi.colour==pj.colour);
            
            forces[i] = forces[i] + f;
            forces[j] = forces[j] - f;
        }
    }
}



void calculate_particle_inter_forces(
    const world_info_t &info,
    unsigned n,
    particle_t *particles,
    vector_t *forces,
    unsigned m,
    const ghost_t *ghosts
){
    for(unsigned i=0; i<n; i++){
        auto &pi=particles[i];
        for(unsigned j=0; j<m; j++){
            auto &gj=ghosts[j];
            vector_t f=force(info, pi.position, gj.position, pi.colour==gj.colour);
            
            forces[i] = forces[i] + f;
        }
    }
}


void threshold_speed(const world_info_t &info, vector_t &v)
{
    real_t speed = v.x * v.x + v.y * v.y;
    if(info.max_speed_squared < speed){
        real_t scale = inv_sqrt(speed);
        v.x = v.x * scale;
        v.y = v.y * scale;
    }
}

// Give compiler the option of inlining or sharing
// axis==0 -> x, axis==1 -> y
// dir==+1, ensure boundary <= pos, dir==-1, pos <= boundary
void reflect(real_t boundary, int axis, int dir, vector_t &pos, vector_t &vel)
{
    const real_t zero=real_t::from_double(0.0);
    
    real_t &p=axis==0 ? pos.x : pos.y;
    real_t &v=axis==0 ? vel.x : vel.y;
    
    real_t delta=(p-boundary);
    bool flip= dir>0 ? delta<zero : zero<delta;
    if(flip){
        p=boundary-delta;
        v = -v;
    }
}

void update_particles(
    const world_info_t &info,
    unsigned n,
    particle_t *particles,
    vector_t *forces
){
    for(unsigned i=0; i<n; i++){
        auto &p = particles[i];
        
        vector_t a = forces[i] * info.dt_over_mass;
        vector_t nv = p.velocity + a * info.dt;
        threshold_speed(info, nv);
        nv=nv*info.drag;

        vector_t np = p.position + nv * info.dt;

        
        reflect( info.left,    0, +1, np, nv);
        reflect( info.right,   0, -1, np, nv);
        reflect( info.bottom,  1, +1, np, nv);
        reflect( info.top,     1, -1, np, nv);
        
        //std::cerr<<i<<" : p="<<p.position<<" v="<<p.velocity<<"   f="<<forces[i]<<"  a'="<<a<<" v'="<<nv<<" p'="<<np<<"\n";
 
        p.velocity = nv;
        p.position = np;
    }
}
