/*******************************************************
                          cache.h
********************************************************/

#ifndef CACHE_H
#define CACHE_H

#include <cmath>
#include <iostream>

typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;

/**** Add new states, based on the protocol ****/
enum Protocol{
   MSI = 0,
   MSI_BUSUPGR,
   MESI,
   MESI_SNOOP_FILTER,
};

enum Bus_Signal{
   BusNoSig = 0,
   BusRd,
   BusRdX,
   BusUpgr,
};

enum {
   INVALID = 0,
   MODIFIED,
   SHARED,
   EXCLUSIVE,

   VALID=10,
   DIRTY,
};

class cacheLine {
   protected:
      ulong tag;
      ulong state;   // 0:invalid, 1:valid, 2:dirty 
      ulong seq; 
   
   public:
      cacheLine()                { tag = 0; state = 0; }
      ulong getTag()             { return tag; }
      ulong getState()           { return state;}
      ulong getSeq()             { return seq; }
      void setSeq(ulong Seq)     { seq = Seq;}
      void setState(ulong flags) {  state = flags;}
      void setTag(ulong a)       { tag = a; }
      void invalidate()          { tag = 0; state = INVALID; } 
      bool isValid()             { return ((state) != INVALID); }
};

class Cache{
   protected:
      ulong size, lineSize, assoc, sets, log2Sets, log2Blk, protocol, tagMask, numLines;
      ulong reads,readMisses,writes,writeMisses,writeBacks;
      ulong proc_num, num_procs;

      //******///
      //coherence counters ///
      ulong c2c_transfers, mem_transactions, interventions, invalidations, flush_cnt, BusRdX_cnt, BusUpgr_cnt;
      ulong useful_snoops_cnt, wasted_snoops_cnt, filtered_snoops_cnt;
      ulong hist_filt_ways, hist_filt_entries;
      //******///

      Cache** cacheArray;
      cacheLine **cache;
      Cache* hist_filter;
      ulong calcTag(ulong addr)     { return (addr >> (log2Blk) );}
      ulong calcIndex(ulong addr)   { return ((addr >> log2Blk) & tagMask);}
      ulong calcAddr4Tag(ulong tag) { return (tag << (log2Blk));}
      
   public:
      ulong currentCycle;  
      
      Cache(int,int,int);
      Cache(Cache**, int,int,int,int,int,int,int,int);
      ~Cache() { delete cache;}
      
      cacheLine* isLineInOtherProcs(ulong);
      cacheLine *findLineToReplace(ulong addr);
      cacheLine *fillLine(ulong addr);
      cacheLine * findLine(ulong addr);
      cacheLine * getLRU(ulong);

      // getters
      ulong getRM()     {return readMisses;} 
      ulong getWM()     {return writeMisses;} 
      ulong getReads()  {return reads;}       
      ulong getWrites() {return writes;}
      ulong getWB()     {return writeBacks;}
      
      void access(ulong, uchar);
      void writeBack(ulong) {writeBacks++; mem_transactions++;};
      void sendBusSig(ulong, ulong);
      void handleBusSignal(ulong, ulong);
      void printStats(ulong procNum);
      void updateLRU(cacheLine *);

};

#endif
