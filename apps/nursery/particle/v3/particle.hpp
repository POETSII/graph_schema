#ifndef particles_hpp
#define particles_hpp

#include <cstdint>
#include <stdexcept>
#include <cmath>
#include <iostream>

struct real_t
{
public:
    static const int frac_bits = 16;
    
private:
    int32_t val;

    constexpr real_t(int32_t _val)
        : val(_val)
    {}

    constexpr real_t check_range(int32_t v, int64_t vx) const 
    {
    #ifndef NDEBUG
        return real_t( (INT32_MIN <= v) && (vx <= INT32_MAX) ? v : throw std::runtime_error("overflow.") );
    #else
        return real_t( v );
    #endif
    }

    static const int64_t raw_half = 1<<(frac_bits-1);
    static const int64_t raw_one = 1<<(frac_bits);

    constexpr int64_t mul64(int64_t a, int64_t b) const
    {
        return ((a*b)+raw_half)>>frac_bits;
    }

    constexpr double inv_helper(int32_t v) const
    {
        return int64_t((1.0/(v/double(raw_one)))*raw_one);
    }

    constexpr double inv_sqrt_helper(int32_t v) const
    {
        return int64_t((1.0/sqrt(v/double(raw_one)))*raw_one);
    }
public:
    constexpr
    real_t()
        : val(0)
    {}

    static constexpr real_t eps()
    { return real_t(1); }

    static constexpr real_t zero()
    { return real_t(0); }
    
    static constexpr real_t one()
    { return real_t(1<<frac_bits); }

    static constexpr real_t from_double(double x)
    { return real_t(int32_t(x* (1<<frac_bits))); }

    constexpr double to_double() const
    { return val/double(raw_one); }

    constexpr real_t operator+(real_t o) const
    {
        return check_range(val+o.val, int64_t(val)+o.val);
    }

    constexpr real_t operator-(real_t o) const
    {
        return check_range(val-o.val, int64_t(val)-o.val);
    }

    constexpr real_t operator-() const
    { return real_t(-val); } // TODO : technically could overflow...

    constexpr real_t operator*(real_t o) const
    {
        return check_range(int32_t(mul64(val,o.val)), mul64(val,o.val));
    }

    constexpr real_t inv() const
    { return check_range(int32_t(inv_helper(val)),inv_helper(val)); }

    constexpr real_t inv_sqrt() const
    { return check_range( int32_t(inv_sqrt_helper(val)), inv_sqrt_helper(val) ); }


    constexpr bool operator<(real_t o) const
    { return val < o.val; }

    constexpr bool operator<=(real_t o) const
    { return val <= o.val; }

    constexpr bool operator>(real_t o) const
    { return val > o.val; }

    constexpr bool operator>=(real_t o) const
    { return val >= o.val; }

    constexpr real_t abs() const
    { return val>=0 ? *this : real_t(-val); }
};

inline constexpr real_t abs(real_t a)
{ return a.abs(); }

inline constexpr real_t inv(real_t a)
{ return a.inv(); }

inline constexpr real_t inv_sqrt(real_t a)
{ return a.inv_sqrt(); }



struct vector_t
{
    real_t x;
    real_t y;
    
    static constexpr vector_t zero() 
    { return vector_t{}; }
    
    static constexpr vector_t from_double(double x, double y)
    { return vector_t{ real_t::from_double(x), real_t::from_double(y) }; }

    constexpr vector_t operator+(const vector_t &o) const
    { return vector_t{ x+o.x, y+o.y}; }

    constexpr vector_t operator-(const vector_t &o) const
    { return vector_t{ x-o.x, y-o.y}; }

    constexpr vector_t operator*(const real_t &o) const
    { return vector_t{ x*o, y*o}; }

};

inline std::ostream &operator<<(std::ostream &dst, const vector_t &v)
{
    return dst<<"("<<v.x.to_double()<<","<<v.y.to_double()<<")";
}


struct particle_t
{
    vector_t position;
    vector_t velocity;
    int32_t colour;
};

struct ghost_t
{
    vector_t position;
    int32_t colour;
};

struct world_info_t
{
    real_t dt;
    real_t mass;
    real_t dt_over_mass;
    
    real_t horizon; // No interactions over more than this distance
    real_t horizon_squared;

    // Max speed
    real_t max_speed;
    real_t max_speed_squared;

    real_t thermal;

    real_t drag;
    
    real_t left, top, right, bottom;

    world_info_t
    (
     double _dt, double _mass,
     double _horizon, double _drag,
     double _thermal
     ){
        dt=real_t::from_double(_dt);
        mass=real_t::from_double(_mass);
        dt_over_mass=real_t::from_double(_dt / _mass);
        horizon=real_t::from_double(_horizon);
        horizon_squared=real_t::from_double(_horizon*_horizon);
        max_speed=real_t::from_double( _horizon * _dt * 0.9 );
        max_speed_squared=real_t::from_double( _horizon*_horizon * _dt*_dt * 0.9*0.9 );
        thermal=real_t::from_double(_thermal);
        drag=real_t::from_double(_drag);
    }
};


// This will zero forces first
void calculate_particle_intra_forces(
    const world_info_t &info,
    unsigned n,
    particle_t *particles,
    vector_t *forces // One for each particle
);

// This does _not_ zero forces, only adds to them
void calculate_particle_inter_forces
(
    const world_info_t &info,
    unsigned n,
    particle_t *particles,
    vector_t *forces,
    unsigned m,
    const ghost_t *ghosts
 );

void update_particles(
    const world_info_t &info,
    unsigned n,
    particle_t *particles,
    vector_t *forces // One for each particle
);


#endif
