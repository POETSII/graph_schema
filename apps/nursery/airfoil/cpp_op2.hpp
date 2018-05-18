#ifndef cpp_op2_hpp
#define cpp_op2_hpp

#include "H5Cpp.h"

#include <cassert>
#include <stdexcept>
#include <vector>
#include <array>


template<class T>
const H5::PredType &getPredType();

template<>
const H5::PredType &getPredType<unsigned>()
{ return H5::PredType::NATIVE_UINT; }

template<>
const H5::PredType &getPredType<int>()
{ return H5::PredType::NATIVE_INT; }

template<>
const H5::PredType &getPredType<float>()
{ return H5::PredType::NATIVE_FLOAT; }

template<>
const H5::PredType &getPredType<double>()
{ return H5::PredType::NATIVE_DOUBLE; }

template<class T>
T read_scalar(H5::CommonFG &file,const char *name)
{
    H5::DataSet set = file.openDataSet(name);
    
    H5::DataSpace space=set.getSpace();
    int rank = space.getSimpleExtentNdims();
    if(rank!=1){
        fprintf(stderr, "Rank = %d\n", rank);
        throw std::runtime_error("Expected rank==1");
    }
    
    hsize_t dims_out[1];
    int ndims = space.getSimpleExtentDims( dims_out, NULL);
    if(dims_out[0]!=1){
        fprintf(stderr, "Size = %llu\n", dims_out[0]);
        throw std::runtime_error("Expected dims==1");
    }
    
    T res;
    set.read(&res, getPredType<T>());
    return res;
}

template<class T>
std::vector<T> read_set(H5::CommonFG &file,const char *name)
{
    unsigned n=read_scalar<unsigned>(file, name);
    fprintf(stderr, "Set[%s] = %u\n", name, n);
    return std::vector<T>(n);
}


template<class TIterType, class TOtherType, unsigned D>
struct op2_map
{
    typedef TIterType iter_type;
    typedef TOtherType other_type;
    
    std::vector<TIterType> &iterSet;
    std::vector<TOtherType> &otherSet;
    std::vector<std::array<unsigned,D> > mapping;
    
    const std::array<unsigned,D> &operator[](unsigned i) const
    {
        assert(i<mapping.size());
        return mapping[i];
    }
};


template<unsigned D, class TA, class TB>
op2_map<TA,TB,D> read_map(H5::CommonFG &file,const char *name, std::vector<TA> &from, std::vector<TB> &to)
{
    H5::DataSet set = file.openDataSet(name);
    
    H5::DataSpace space=set.getSpace();
    int rank = space.getSimpleExtentNdims();
    if(rank!=2){
        fprintf(stderr, "Rank = %d\n", rank);
        throw std::runtime_error("Expected rank==2");
    }
    
    typedef unsigned T;
    unsigned n=from.size();
    
    hsize_t dims_out[2];
    int ndims = space.getSimpleExtentDims( dims_out, NULL);
    if(dims_out[0]!=n){
        fprintf(stderr, "Size[0] = %llu\n", dims_out[0]);
        throw std::runtime_error("Expected different dim1");
    }
    if(dims_out[1]!=D){
        fprintf(stderr, "Size[0] = %llu\n", dims_out[1]);
        throw std::runtime_error("Expected different dim2");
    }
    
    std::vector<std::array<T,D> > res(n);
    set.read(&res[0], getPredType<T>());
    
    unsigned nt=to.size();
    
    for(unsigned i=0; i<n; i++){
        for(unsigned j=0; j<D; j++){
            if(res[i][j]>=nt){
                throw std::runtime_error("Invalid map index.");
            }
        }
    }
    
    return op2_map<TA,TB,D>{from, to, res};
}


template<class T, class M, class TVal, unsigned D>
struct set_dat
{
    static void exec(T &dst, M member, const std::array<TVal,D> &x)
    { dst.*member=x; }
};

template<class T, class M, class TVal>
struct set_dat<T,M,TVal,1>
{
    static void exec(T &dst, M member, const std::array<TVal,1> &x)
    { dst.*member=x[0]; }
};


template<class TSet, class TVal, unsigned D, class TMember>
void read_dat(H5::CommonFG &file,const char *name, std::vector<TSet> &toSet, TMember member)
{
    H5::DataSet set = file.openDataSet(name);
    
    H5::DataSpace space=set.getSpace();
    int rank = space.getSimpleExtentNdims();
    if(rank!=2){
        fprintf(stderr, "Rank = %d\n", rank);
        throw std::runtime_error("Expected rank==2");
    }
    
    unsigned n=toSet.size();
    
    hsize_t dims_out[2];
    int ndims = space.getSimpleExtentDims( dims_out, NULL);
    if(dims_out[0]!=n){
        fprintf(stderr, "Size[0] = %llu\n", dims_out[0]);
        throw std::runtime_error("Expected different dim1");
    }
    if(dims_out[1]!=D){
        fprintf(stderr, "Size[0] = %llu\n", dims_out[1]);
        throw std::runtime_error("Expected different dim2");
    }
    
    std::vector<std::array<TVal,D> > res(n);
    set.read(&res[0], getPredType<TVal>());
        
    for(unsigned i=0; i<n; i++){
        set_dat<TSet,TMember,TVal,D>::exec(toSet[i], member, res[i]);
    }
}

template<class TVal, unsigned long D>
void read_global_impl(H5::CommonFG &file,const char *name, std::array<TVal,D> &val)
{
    H5::DataSet set = file.openDataSet(name);
    
    H5::DataSpace space=set.getSpace();
    int rank = space.getSimpleExtentNdims();
    if(rank!=1){
        fprintf(stderr, "Rank = %d\n", rank);
        throw std::runtime_error("Expected rank==1");
    }
    
    hsize_t dims_out[1];
    int ndims = space.getSimpleExtentDims( dims_out, NULL);
    if(dims_out[0]!=D){
        fprintf(stderr, "Size[0] = %llu\n", dims_out[0]);
        throw std::runtime_error("Expected different dim1");
    }
    
    set.read(&val[0], getPredType<TVal>());
}

template<class TVal, unsigned long D>
void read_global(H5::CommonFG &file,const char *name, std::array<TVal,D> &val)
{
    read_global_impl(file,name,val);
}

template<class TVal>
void read_global(H5::CommonFG &file,const char *name, TVal &val)
{
    std::array<TVal,1> tmp;
    read_global_impl(file,name,tmp);
    val=tmp[0];
}




#endif
