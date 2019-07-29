#define CL_HPP_ENABLE_EXCEPTIONS
#include <CL/cl2.hpp>

#include <iostream>
#include <fstream>
#include <streambuf>
#include <random>
#include <regex>

std::string read_file(const std::string &path)
{
    std::ifstream src(path.c_str());
    if(!src.is_open()){
        throw std::runtime_error("Couldn't open "+path);
    }
    return std::string((std::istreambuf_iterator<char>(src)), std::istreambuf_iterator<char>());
}

int main(int argc, char *argv[])
{
    if(argc<2){
        fprintf(stderr, "Missing kernel file name.\n");
        exit(1);
    }
    std::string path=argv[1];


    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    cl::Platform platform;
    for (auto &p : platforms) {
        /* std::string platver = p.getInfo<CL_PLATFORM_VERSION>();
        if (platver.find("OpenCL 2.") != std::string::npos) {
            plat = p;
        }*/
        platform=p;
        break;
    }
    if (platform() == 0)  {
        std::cerr << "No platform found.";
        return -1;
    }


    std::cerr<<"Getting devices.\n";
    std::vector<cl::Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_DEFAULT, &devices);
    for(auto &d : devices){
        std::cerr<<d.getInfo<CL_DEVICE_NAME>()<<"\n";

        fprintf(stderr, "Checking SVM...\n");
        auto caps=d.getInfo<CL_DEVICE_SVM_CAPABILITIES>();
        if(caps&CL_DEVICE_SVM_COARSE_GRAIN_BUFFER){
            fprintf(stderr, "Course grain buffer\n");
        }
        if(caps&CL_DEVICE_SVM_FINE_GRAIN_BUFFER){
            fprintf(stderr, "Fine grain buffer\n");
        }
        if(caps&CL_DEVICE_SVM_FINE_GRAIN_SYSTEM){
            fprintf(stderr, "Fine grain system\n");
        }
        if(caps&CL_DEVICE_SVM_ATOMICS){
            fprintf(stderr, "Atomics\n");
        }
    }

    //std::string kernel_source(read_file("kernel_instance_3dev.cl"));
    std::string kernel_source(read_file(path));

    std::smatch m;
    if(!std::regex_search(kernel_source, m, std::regex("__TOTAL_DEVICES__=([0-9]+);"))){
        fprintf(stderr, "Couldn't find __TOTAL_DEVICES__ string in open cl source.");
        exit(1);
    }

    const int NUM_DEVICES=std::stoi(m[1]);
    std::cerr<<"Num devices="<<NUM_DEVICES<<"\n";

    cl::Context context(devices);
    
    std::vector<std::string> programStrings { kernel_source };
    std::cerr<<"Constructing program.\n";
    cl::Program program(context, programStrings);
    try {
        std::cerr<<"Building program.\n";
        program.build(devices, "-cl-std=CL2.0");
    }
    catch (...) {
        // Print build info for all devices
        cl_int buildErr = CL_SUCCESS;
        auto buildInfo = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(&buildErr);
        for (auto &pair : buildInfo) {
            std::cerr << pair.second << std::endl << std::endl;
        }
        return 1;
    }

    std::cerr<<"Creating functors\n";
    auto f_init= cl::KernelFunctor<>(program, "kinit");
    auto f_step= cl::KernelFunctor<unsigned>(program, "kstep");

    auto k_init= cl::Kernel(program, "kinit");
    auto k_step= cl::Kernel(program, "kstep");

    cl::CommandQueue queue(context, devices[0]);

    std::cerr<<"Calling init\n";
    queue.enqueueNDRangeKernel(k_init, cl::NDRange(0), cl::NDRange(NUM_DEVICES));
    queue.finish();
    std::cerr<<"Init done.\n";


    std::mt19937 rng;

    k_step.setArg(0, (uint32_t)1);

    for(unsigned i=0; i<1000; i++){
        std::cerr<<"Step "<<i<<"\n";
        
        queue.enqueueNDRangeKernel(k_step, cl::NDRange(0), cl::NDRange(NUM_DEVICES));
        queue.enqueueBarrierWithWaitList();
        queue.finish();
        //f_step( cl::EnqueueArgs(cl::NDRange(width)), count).wait();
    }

    queue.finish();
}
