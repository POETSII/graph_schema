#include "particle.hpp"

#include <random>

int main()
{
    std::mt19937 urng;
    auto unif=[&](){
        std::uniform_real_distribution<> u;
        return u(urng);
    };
    
    const unsigned N=4096;
    
    const double dt=1.0/64;
    const double mass=1.0;

    const double horizon=1.0;
    const double drag=0.9;
    const double thermal=0.1;
    world_info_t info(dt, mass, horizon, drag, thermal);
    info.left=real_t::from_double(-0.5);
    info.right=real_t::from_double(2.5);
    info.top=real_t::from_double(2.5);
    info.bottom=real_t::from_double(-0.5);

    
    particle_t particles[N];

    for(unsigned i=0; i<N; i++){
        particles[i].position=vector_t::from_double(unif(),unif());
        particles[i].velocity=vector_t::from_double(unif()*0.1,unif()*0.1);
        particles[i].colour=urng()%3;
    }    

    vector_t forces[N];
    for(unsigned i=0; i<N; i++){
        forces[i]=vector_t::zero();
    }

    int steps=0;
    while(1){
       
        calculate_particle_intra_forces(
            info,
            N,
            particles,
            forces
        );
        
        update_particles(
            info,
            N,
            particles,
            forces
        );

        for(unsigned i=0; i<N; i++){
            std::cout<<steps<<","<<steps*dt<<", "<<i<<","<<particles[i].colour<<","<<particles[i].position.x.to_double()<<","<<particles[i].position.y.to_double()<<","<<particles[i].velocity.x.to_double()<<","<<particles[i].velocity.y.to_double()<<"\n";
        }

        std::cout.flush();

        if((steps%10)==0){
            std::cerr<<"Step "<<steps<<"\n";
        }

        if(++steps>2000){
            break;
        }
    }    
}
