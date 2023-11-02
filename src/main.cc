/*******************************************************
                          main.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include <fstream>
#include <map>
using namespace std;

#include "cache.h"

int main(int argc, char *argv[])
{
    
    ifstream fin;
    FILE * pFile;
    map<int, string> prot_map;

    if(argc != 7){
        printf("input format: ");
        printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
        printf("Ex: \n./smp_cache 8192 8 64 4 0 ../trace/canneal.04t.debug\n");
        exit(0);
    }

    prot_map.insert(pair<int, string>(0, "MSI"));
    prot_map.insert(pair<int, string>(1, "MSI BusUpgr"));
    prot_map.insert(pair<int, string>(2, "MESI"));
    prot_map.insert(pair<int, string>(3, "MESI Filter BusNOP"));

    ulong hist_filt_entries = 16;
    ulong hist_filt_ways = 1;

    ulong cache_size        = atoi(argv[1]);
    ulong cache_assoc       = atoi(argv[2]);
    ulong blk_size          = atoi(argv[3]);
    ulong num_processors    = atoi(argv[4]);
    ulong protocol          = atoi(argv[5]); /* 0:MSI 1:MSI BusUpgr 2:MESI 3:MESI Snoop FIlter */
    char *fname             = (char *) malloc(30);
    fname                   = argv[6];


    if(protocol < 0 || protocol > 3){
        printf("Error: Protocol Not Implemented\n");
        exit(EXIT_FAILURE);
    }

    printf("===== Coherence Simulator configuration =====\n");
    printf("L1_SIZE: %lu\n", cache_size);
    printf("L1_ASSOC: %lu\n", cache_assoc);
    printf("L1_BLOCKSIZE: %lu\n", blk_size);
    printf("NUMBER OF PROCESSORS: %lu\n", num_processors);
    cout << "COHERENCE PROTOCOL: " << prot_map[protocol] << endl;
    printf("TRACE FILE: %s\n\n", fname);
    
    // Using pointers so that we can use inheritance */
    Cache** cacheArray = (Cache **) malloc(num_processors * sizeof(Cache));
    for(ulong i = 0; i < num_processors; i++) {
        cacheArray[i] = new Cache(cacheArray, cache_size, cache_assoc, blk_size, 
                            protocol, i, num_processors, hist_filt_ways, hist_filt_entries);
    }

    pFile = fopen (fname,"r");
    if(pFile == 0){   
        printf("Trace file problem\n");
        exit(0);
    }
    
    ulong proc;
    char op;
    ulong addr;

    while(fscanf(pFile, "%lu %c %lx", &proc, &op, &addr) != EOF){
#ifdef _DEBUG
        static int line = 1;
        printf("%d\n", line);
        line++;
#endif
        cacheArray[proc]->access(addr, op);
    }

    fclose(pFile);

    // Printing caches statistics
    for(ulong i = 0; i < num_processors; i++){
        cacheArray[i]->printStats(i);
        printf("\n");
    }
    
}
