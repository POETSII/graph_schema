#ifndef neuron_hpp
#define neuron_hpp

#include <cstdint>
#include <memory>
#include <functional>

#include "dumb_snn_sink.hpp"

using stimulus_type = int32_t;

class Neuron
{
public:
    virtual ~Neuron()
    {}

    virtual void reset(uint64_t seed) =0;

    virtual bool step(float dt, unsigned nStim ,const stimulus_type *pStim) =0;

    virtual float project() const =0;
};

using neuron_factory_t = std::function<
    std::shared_ptr<Neuron>(const prototype &p, std::string_view id, unsigned nParams, const double *pParams)
>;

neuron_factory_t create_neuron_factory(const prototype &p);

#endif
