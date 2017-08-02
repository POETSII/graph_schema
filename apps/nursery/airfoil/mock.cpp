#include "H5Cpp.h"

#include <cassert>
#include <stdexcept>

typedef float working_t;

struct Node
{
    working_t x[2];
};

struct Edge
{
 
};

struct BEdge
{
    bool bound;
};

struct Cell
{
    float q[4];
    float qold[4];
    float adt;
    float res[4];
};

template<class T>
const H5::PredType &getPredType();

template<>
const H5::PredType &getPredType<unsigned>()
{ return H5::PredType::NATIVE_UINT; }

template<>
const H5::PredType &getPredType<float>()
{ return H5::PredType::NATIVE_FLOAT; }

template<class T>
T read_scalar(H5::CommonFG &file,const char *name)
{
    H5::DataSet set = file.openDataSet(name);
    
    H5::DataSpace space=set.getSpace();
    int rank = space.getSimpleExtentNdims();
    if(rank!=1){
        fprintf(stderr, "Rank = %u\n", rank);
        throw std::runtime_error("Expected rank==1");
    }
    
    hsize_t dims_out[1];
    int ndims = space.getSimpleExtentDims( dims_out, NULL);
    if(dims_out[0]!=1){
        fprintf(stderr, "Size = %u\n", dims_out[0]);
        throw std::runtime_error("Expected dims==1");
    }
    
    T res;
    set.read(&res, getPredType<T>());
    return res;
}

int main(int argc, char *argv[])
{
    const char *srcFile=argv[1];
    
    H5::H5File file(srcFile, H5F_ACC_RDONLY);
    
    unsigned numCells=read_scalar<unsigned>(file, "cells");
    unsigned numNodes=read_scalar<unsigned>(file, "nodes");
    unsigned numEdges=read_scalar<unsigned>(file, "edges");
    unsigned numBEdges=read_scalar<unsigned>(file, "bedges");
}
