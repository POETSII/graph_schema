#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <boot.h>
#include <config.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "HostLink.h"

// Send file contents over host-link
uint32_t sendFile(BootCmd cmd, HostLink* link, FILE* fp, uint32_t* checksum, int verbosity)
{
  // Iterate over data file
  uint32_t addr = 0;
  uint32_t value = 0;
  uint32_t byteCount = 0;
  uint32_t byte = 0;
  for (;;) {
    // Send write address
    if (fscanf(fp, " @%x", &addr) > 0) {
      if(verbosity>1){
	fprintf(stderr, "    Writing to address 0x%08x\n", addr);
      }
      link->put(SetAddrCmd);
      link->put(addr);
      *checksum += SetAddrCmd + addr;
    }
    else break;
    // Send write values
    uint32_t currAddr=addr;
    while (fscanf(fp, " %x", &byte) > 0) {
      value = (byte << 24) | (value >> 8);
      byteCount++;
      if (byteCount == 4) {
	if(verbosity>2){
	  fprintf(stderr, "      Write: mem[0x%08x] = 0x%08x\n", currAddr, value);
	}
        link->put(cmd);
	if(verbosity>3){
	  fprintf(stderr, "        wrote command\n");
	}
        link->put(value);
	if(verbosity>3){
	  fprintf(stderr, "        wrote value\n");
	}
        *checksum += cmd + value;
        value = 0;
        byteCount = 0;
	currAddr+=4;
      }
    }
    // Pad & send final word, if necessary
    if (byteCount > 0) {
      while (byteCount < 4) {
        value = value >> 8;
        byteCount++;
      }
      if(verbosity>2){
	fprintf(stderr, "      Write: mem[0x%08x] = 0x%08x\n", currAddr, value);
      }
      link->put(cmd);
      link->put(value);
      *checksum += cmd + value;
      byteCount = 0;
    }
  }
  return addr;
}

double now()
{
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return tp.tv_sec+1e-9*tp.tv_nsec;
}

int usage()
{
  printf("Usage:\n"
         "  hostlink [CODE] [DATA]\n"
         "    -o            start only one thread\n"
         "    -n [NUMBER]   num messages to dump after boot\n"
         "    -t [SECONDS]  timeout on message dump\n"
         "    -v            increase verbosity\n"
         "    -h            help\n"
         "\n"
  return -1;
}



struct sm
{
    sm(unsigned _thread)
        : thread(_thread)
    {}
    
    unsigned thread;
    unsigned state=0;
    uint32_t data[4];
    
    bool operator()(uint32_t x)
    {
        if(state==-1){
            throw std::runtime_error("Received word after thread finished.");
        }
        if(x==-1){
            state=-1;
            return false;
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
            fprintf(stdout, "%d, %f, %d, %d, %f, %f, 0, 0, %d\n", steps, steps/64.0, id, colour, xpos, ypos, thread);
            state=0;
        }
        return true;
    }
};


int main(int argc, char* argv[])
{
  int numMessages = -1;
  int numSeconds = -1;
  int verbosity = 0;

  FILE *keyValDst = 0;
  FILE *measureDst = 0;
  FILE *perfmonDst = 0;

  // Option processing
  optind = 1;
  for (;;) {
    int c = getopt(argc, argv, "hon:t:cpk:i:m:v");
    if (c == -1) break;
    switch (c) {
      case 'h': return usage();
      case 'n': numMessages = atoi(optarg); break;
      case 't': numSeconds = atoi(optarg); break;
      case 'v': verbosity++; break;
	break;
      }
      default: return usage();
    }
  }
  if (optind+2 != argc) return usage();

  if(verbosity>0){
    fprintf(stderr, "Reading code from '%s'\n", argv[optind]);
  }
  // Open code file
  FILE* code = fopen(argv[optind], "r");
  if (code == NULL) {
    printf("Error: can't open file '%s'\n", argv[optind]);
    return -1;
  }

  if(verbosity>0){
    fprintf(stderr, "Reading data from '%s'\n", argv[optind+1]);
  }
  // Open data file
  FILE* data = fopen(argv[optind+1], "r");
  if (data == NULL) {
    printf("Error: can't open file '%s'\n", argv[optind+1]);
    exit(EXIT_FAILURE);
  }

  // State
  HostLink link;
  uint32_t checksum = 0;

  // Step 1: load code into instruction memory
  // -----------------------------------------
  double start=now();
  fseek(code, 0, SEEK_END);
  unsigned codeSize=ftell(code);
  rewind(code);
  if(verbosity>0){  fprintf(stderr, "Loading code into instruction memory, size = %u bytes\n", codeSize);  }

  if (startOnlyOneThread)
    // Write instructions to core 0 only
    link.setDest(0x00000000);
  else
    // Broadcast instructions to all cores
    link.setDest(0x80000000);

  // Write instructions to instruction memory
  uint32_t instrBase = sendFile(WriteInstrCmd, &link, code, &checksum, verbosity);

  double finish=now();
  if(measureDst){
    fprintf(measureDst, "hostlinkLoadInstructions, -, %f, sec\n", finish-start);
    fflush(measureDst);
  }
  start=now();

  // Step 2: initialise memory using data file
  // -----------------------------------------
  fseek(data, 0, SEEK_END);
  unsigned dataSize=ftell(data);
  rewind(data);
  if(verbosity>0){  fprintf(stderr, "Initialise memory using data file, size = %u bytes\n", dataSize);  }

  // Iterate over each DRAM
  uint32_t coresPerDRAM =
             1 << (TinselLogCoresPerDCache + TinselLogDCachesPerDRAM);
  for (int i = 0; i < TinselDRAMsPerBoard; i++) {
    if(verbosity>1){  fprintf(stderr, "  Initialising DRAM %u\n", i);  }
    // Use one core to initialise each DRAM
    link.setDest(coresPerDRAM * i);

    // Write data file to memory
    if(verbosity>1){  fprintf(stderr, "    sending file to memory\n");  }
    uint32_t ignore;
    rewind(data);
    sendFile(StoreCmd, &link, data, i == 0 ? &checksum : &ignore, verbosity);

    // Send cache flush
    if(verbosity>1){  fprintf(stderr, "    sending cache flush\n");  }
    link.put(CacheFlushCmd);
    if (i == 0) checksum += CacheFlushCmd;

    // Obtain response and validate checksum
    if(verbosity>1){  fprintf(stderr, "    obtaining response and validating checksum\n");  }
    uint32_t src;
    uint32_t sum;
    link.get(&src, &sum);

    if (sum != checksum) {
      printf("Error: data checksum failure from core %d ", src);
      printf("(0x%x v. 0x%x)\n", checksum, sum);
      exit(EXIT_FAILURE);
    }

    if (startOnlyOneThread) break;
  }

  finish=now();
  if(measureDst){
    fprintf(measureDst, "hostlinkLoadData, -, %f, sec\n", finish-start);
    fflush(measureDst);
  }
  start=now();

  // Step 3: release the cores
  // -------------------------
 if(verbosity>0){  fprintf(stderr, "Releasing the cores\n");  }

  if (startOnlyOneThread)
    // Send start command to core 0 only
    link.setDest(0);
  else
    // Broadcast start commands
    link.setDest(0x80000000);

  // Send start command with initial program counter
  uint32_t numThreads = startOnlyOneThread ? 1 : (1 << TinselLogThreadsPerCore);
  link.put(StartCmd);
  link.put(numThreads);
  checksum += StartCmd + numThreads;
  
  int W=4, H=4;
  
  link.setDest(0x80000000);
  link.put(W);
  link.put(H);
  link.put(particlesPerCell);
  
  unsigned finished=0;
  std::vector<sm> sms;
  
  // Send setup info to threads
  for(unsigned y=0; y<4; y++){
    for(unsigned x=0; x<4; x++){
      link.setDest(y*W+x);
      link.put(x);
      link.put(y);
      assert(y*W+x==sms.size());
      sms.push_back( sm{ sms.size(); } );
    }
  }
  
  link.setDest(0x80000000);
  link.put(timeSteps);
  link.put(outputDeltaSteps);
  
  start=now();
  
  // The number of tenths of a second that link has been idle
  while(finished < W*H){
    while (! link.canGet()) {
      usleep(100000);
    }
     
    uint32_t src, val;
    uint8_t cmd = link.get(&src, &val);
    printf("%d %x %x\n", src, cmd, val);
    
    bool more=sms.at(src)(val);
    if(!more){
      finished++;
      fprintf(stderr, "Thread %d finished\n", src);
    }
  }

  return 0;
}
