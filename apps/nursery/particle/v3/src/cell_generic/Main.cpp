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
#include <cmath>

#include "parameters.hpp"

double now()
{
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return tp.tv_sec+1e-9*tp.tv_nsec;
}


#include "state_machine.hpp"

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


int usage()
{
  printf("Usage:\n"
         "  hostlink [CODE] [DATA]\n"
         "    -o            start only one thread\n"
         "    -n [NUMBER]   num messages to dump after boot\n"
         "    -t [SECONDS]  timeout on message dump\n"
         "    -v            increase verbosity\n"
         "    -h            help\n"
         "\n");
  return -1;
}


int main(int argc, char* argv[])
{
  int numMessages = -1;
  int numSeconds = -1;
  int verbosity = 0;

  // Option processing
  optind = 1;
  for (;;) {
    int c = getopt(argc, argv, "hn:t:v");
    if (c == -1) break;
    switch (c) {
      case 'h': return usage();
      case 'n': numMessages = atoi(optarg); break;
      case 't': numSeconds = atoi(optarg); break;
      case 'v': verbosity++; break;
      default: return usage();
    }
  }
  if (optind+2 != argc){
	fprintf(stderr, "Not enough args.");
	 return usage();
  }

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

  // Broadcast instructions to all cores
  link.setDest(0x80000000);

  // Write instructions to instruction memory
  uint32_t instrBase = sendFile(WriteInstrCmd, &link, code, &checksum, verbosity);

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
  }

  // Step 3: release the cores
  // -------------------------
 if(verbosity>0){  fprintf(stderr, "Releasing the cores\n");  }

    /// Broadcast start commands
    link.setDest(0x80000000);

  
///////////////////////////////////////////////////////
// End generic startup

// Begin app-specific
////////////////////////////////////////////////////////

  g_start=now();

  int W=CELL_W, H=CELL_H;
  int particlesPerCell=PARTICLES_PER_CELL;
  int timeSteps=TIME_STEPS;
  int outputDeltaSteps=OUTPUT_DELTA;

  unsigned finished=0;
  // No std::vector because... altera

  sm *sms=(sm*)malloc(sizeof(sm)*32*32);


  int *inLoValid=(int*)malloc(4*32*32);
   memset(inLoValid, 0, 4*32*32);
  uint32_t *inLo=(uint32_t*)malloc(4*32*32);

  auto do_input=[&]()
  {
    if(link.canGet()){
       uint32_t src, val;
       uint8_t cmd = link.get(&src, &val);

	uint32_t tid=val>>16; 
        val=val&0xFFFF;
        if(W*H <= tid){
	       fprintf(stderr, "Got: %d %x %x, thread=%u, payload=%u\n", src, cmd, val, val>>16, val&0xFFFF);
		exit(1);
	}

        if(inLoValid[tid]){
           val=(val<<16) | inLo[tid];
	
       	   bool more=sms[tid](val);
	   if(!more){
               finished++;
               fprintf(stderr, "Thread %d finished, finished=%u\n", tid, finished);
           }
	   inLoValid[tid]=0;
        }else{
	   inLo[tid]=val;
           inLoValid[tid]=1;
	}    	
    }
  };

/*
  auto put=[&](uint32_t x)
  {
    while(link.canGet()){
       do_input();
    }
    link.put(x);
  };
*/


  if(verbosity>1){ fprintf(stderr, "Setting up per-thread info.\n"); }  
  for(unsigned y=0; y<H; y++){
    for(unsigned x=0; x<W; x++){
      sms[y*W+x].thread=y*W+x;
    }
  }  


   /////////////////////////////////////////
//
//  This is from the generic startup
  // Send start command with initial program counter
  uint32_t numThreads = (1 << TinselLogThreadsPerCore);
  link.put(StartCmd);
  link.put(numThreads);
  checksum += StartCmd + numThreads;
  
  start=now();


  if(verbosity>1){ fprintf(stderr, "Collecting input.\n"); }  
  // The number of tenths of a second that link has been idle
  while(finished < 1){ // W*H){
    while (! link.canGet()) {
      double elapsed=now()-start;
      if( (numSeconds!=-1) && ( elapsed > numSeconds )){
        if(verbosity>0){ fprintf(stderr, "Aborting with timeout after %f seconds", elapsed); }
        exit(134);
      }
      usleep(10000);
    }
    do_input();
     
  }

  double finish=now();
  double onChip=sms[0].elapsed_timestamp;
  fprintf(stdout, "%u, %u, %u, %u, %u, %g, %g, %g\n",
	CELL_W, CELL_H, particlesPerCell, timeSteps, outputDeltaSteps, g_finish-g_start, finish-start, onChip);


  return 0;
}
