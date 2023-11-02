/*******************************************************
                          cache.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include "cache.h"
using namespace std;

Cache::Cache(Cache** cA, int s,int a,int b, int p, int pn, int np, int hfw, int hfe)
{
   ulong i, j;
   reads = readMisses = writes = 0; 
   writeMisses = writeBacks = currentCycle = 0;
   c2c_transfers = mem_transactions = interventions = invalidations = 0;
   flush_cnt = BusRdX_cnt = BusUpgr_cnt = 0;
   useful_snoops_cnt = wasted_snoops_cnt = filtered_snoops_cnt = 0;

   cacheArray = cA;
   proc_num = pn;
   num_procs = np;

   protocol   = p;
   size       = (ulong)(s);
   lineSize   = (ulong)(b);
   assoc      = (ulong)(a);   
   sets       = (ulong)((s/b)/a);
   numLines   = (ulong)(s/b);
   log2Sets   = (ulong)(log2(sets));   
   log2Blk    = (ulong)(log2(b));   

   //tagMask =0;
   //for(i=0;i<log2Sets;i++)
   //{
   //   tagMask <<= 1;  
   //   tagMask |= 1;
   //}
   tagMask = ~(~0u << log2Sets);
   
   /** Create a two dimentional cache, sized as cache[sets][assoc] **/ 
   cache = new cacheLine*[sets];
   for(i=0; i<sets; i++){
      cache[i] = new cacheLine[assoc];
      for(j=0; j<assoc; j++){
         cache[i][j].invalidate();
      }
   }      

   hist_filter = new Cache(log2Blk, hfw, hfe);
   
}

// Alternate Cache constructor for the history filter table
Cache::Cache(int l2B, int hfw, int hfe){
   currentCycle = 0;
   assoc = hfw;
   currentCycle = 0;
   log2Blk    = l2B;   
   log2Sets   = (ulong)(log2(hfe));   
   tagMask = ~(~0u << log2Sets);

   /** Create a two dimentional cache, sized as cache[sets][assoc] **/ 
   cache = new cacheLine*[hfe];
   for(int i=0; i<hfe; i++){
      cache[i] = new cacheLine[hfw];
      for(int j=0; j<hfw; j++){
         cache[i][j].invalidate();
      }
   }      
}

/** Entry point to the memory hierarchy (i.e. caches) **/
void Cache::access(ulong addr, uchar op){
   currentCycle++;/*per cache global counter to maintain LRU order 
                    among cache ways, updated on every cache access*/
   
   cacheLine * line_hist_filter;
         
   if(op == 'w') writes++;
   else          reads++;
   
   cacheLine * line = findLine(addr);

   if(line == NULL){ /*miss*/
      if(op == 'w') writeMisses++;
      else readMisses++;

      cacheLine *newline = fillLine(addr);

      switch (protocol){
         case MSI:
         case MSI_BUSUPGR:
            if(op == 'w'){ 
               sendBusSig(addr, BusRdX);
               newline->setState(MODIFIED);    
               BusRdX_cnt++;
            }else{
               sendBusSig(addr, BusRd);
               newline->setState(SHARED);
            }
            break;

         case MESI:
         case MESI_SNOOP_FILTER:
            if(op == 'w'){ 
               sendBusSig(addr, BusRdX);
               newline->setState(MODIFIED);    
               BusRdX_cnt++;
            }else{

               sendBusSig(addr, BusRd);
               if (isLineInOtherProcs(addr) != NULL){
                  newline->setState(SHARED);
               }else{
                  newline->setState(EXCLUSIVE);
               }
            }
            line_hist_filter = hist_filter->findLine(addr);
            if(line_hist_filter != NULL){
               line_hist_filter->invalidate();
            }
            break;

         default: // basic_no_coherence
            printf("Prot Not Supported 0\n");
            exit(EXIT_FAILURE);
            break;
      }
      
   }else{ // cache hit
      /** Update LRU and update state **/
      updateLRU(line);

      switch (protocol){
         case MSI:
            if(line->getState() == SHARED){
               if(op == 'w'){
                  sendBusSig(addr, BusRdX);
                  line->setState(MODIFIED);
                  BusRdX_cnt++;
                  mem_transactions++;
               }
            }
            break;

         case MSI_BUSUPGR:
            if(line->getState() == SHARED){
               if(op == 'w'){
                  sendBusSig(addr, BusUpgr);
                  line->setState(MODIFIED);
                  BusUpgr_cnt++;
               }
            }
            break;

         case MESI:
         case MESI_SNOOP_FILTER:
            if(line->getState() == SHARED){
               if(op == 'w'){
                  sendBusSig(addr, BusUpgr);
                  line->setState(MODIFIED);
                  BusUpgr_cnt++;
               }
            }else if(line->getState() == EXCLUSIVE){
               if(op == 'w'){
                  line->setState(MODIFIED);
               }
            }
            line_hist_filter = hist_filter->findLine(addr);
            if(line_hist_filter != NULL){
               line_hist_filter->invalidate();
            }
            break;

         default: // basic_no_coherence
            printf("Prot Not Supported 1\n");
            exit(EXIT_FAILURE);
            break;
      }
   }
}

void Cache::sendBusSig(ulong addr, ulong bus_sig){

   for(ulong i = 0; i < num_procs; i++){
      if (i == proc_num) continue;

      cacheArray[i]->handleBusSignal(addr, bus_sig);
   }

}

void Cache::handleBusSignal(ulong addr, ulong bs){

   cacheLine * line = findLine(addr);

   if (line == NULL){
      if (hist_filter->findLine(addr) == NULL){
         wasted_snoops_cnt++;
         hist_filter->fillLine(addr);
      }else{
         filtered_snoops_cnt++;
      }

   }else{ // cache hit
      useful_snoops_cnt++;

      if (protocol == MSI){
         switch (line->getState()){
            case SHARED:
               if(bs == BusRdX){
                  line->invalidate();
                  invalidations++;
               }
               break;

            case MODIFIED:
               if(bs == BusRd){
                  line->setState(SHARED);
                  flush_cnt++;
                  writeBack(addr);
                  interventions++;
               }else if(bs == BusRdX){
                  line->invalidate();
                  flush_cnt++;
                  writeBack(addr);
                  invalidations++;
               }
               break;

            default:
               break;
         }

      }else if (protocol == MSI_BUSUPGR){
         switch (line->getState()){
            case SHARED:
               if(bs == BusRdX || bs == BusUpgr){
                  line->invalidate();
                  invalidations++;
               }
               break;

            case MODIFIED:
               if(bs == BusRd){
                  line->setState(SHARED);
                  flush_cnt++;
                  writeBack(addr);
                  interventions++;
               }else if(bs == BusRdX){
                  line->invalidate();
                  flush_cnt++;
                  writeBack(addr);
                  invalidations++;
               }
               break;

            default:
               break;
         }
      }else if (protocol == MESI || protocol == MESI_SNOOP_FILTER){

         switch (line->getState()){
            case SHARED:
               if(bs == BusRdX || bs == BusUpgr){
                  line->invalidate();
                  hist_filter->fillLine(addr);
                  invalidations++;
               }
               break;

            case MODIFIED:
               if(bs == BusRd){
                  line->setState(SHARED);
                  flush_cnt++;
                  writeBack(addr);
                  interventions++;
               }else if(bs == BusRdX){
                  line->invalidate();
                  hist_filter->fillLine(addr);
                  flush_cnt++;
                  writeBack(addr);
                  invalidations++;
               }
               break;
            
            case EXCLUSIVE:
               if(bs == BusRd){
                  line->setState(SHARED);
                  interventions++;
               }else if(bs == BusRdX){
                  line->invalidate();
                  hist_filter->fillLine(addr);
                  invalidations++;
               }
               break;


            default:
               break;
         }

      }else{ // basic_no_coherence
            printf("Prot Not Supported 2");
            exit(EXIT_FAILURE);
      }   
   }
}

/* Look up line */
cacheLine * Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;
   
   pos = assoc; 
   tag = calcTag(addr);
   i   = calcIndex(addr);
  
   for(j=0; j<assoc; j++){
      if(cache[i][j].isValid()) {
         if(cache[i][j].getTag() == tag)
         {
            pos = j; 
            break; 
         }
      }
   }
   if(pos == assoc) { //out of bound, no match.
      return NULL;
   }else {
      return &(cache[i][pos]); 
   }
}

/* Upgrade LRU line to be MRU line */
void Cache::updateLRU(cacheLine *line)
{
   line->setSeq(currentCycle);  
}

/* Return an invalid line as LRU, if any, otherwise return LRU line */
cacheLine * Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min    = currentCycle;
   i      = calcIndex(addr);
   
   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].isValid() == 0) { 
         return &(cache[i][j]); 
      }   
   }

   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].getSeq() <= min) { 
         victim = j; 
         min = cache[i][j].getSeq();}
   } 

   assert(victim != assoc);
   
   return &(cache[i][victim]);
}

/* Find a victim, move it to MRU position */
cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine * victim = getLRU(addr);
   updateLRU(victim);
  
   return (victim);
}

cacheLine * Cache::isLineInOtherProcs(ulong addr){

   for(ulong i = 0; i < num_procs; i++){
      if (i == proc_num) continue;

      cacheLine * line = cacheArray[i]->findLine(addr);
      if(line != NULL){
         return line;
      }
   }

   return NULL;
}


/* Allocate a new line */
cacheLine *Cache::fillLine(ulong addr){ 
   ulong tag;
  
   cacheLine *victim = findLineToReplace(addr);
   assert(victim != 0);
   
   if(victim->getState() == MODIFIED){
      writeBack(addr);
   }

   // Allocation: grabbing block from mem OR other cache
   switch (protocol){
      case MESI:
      case MESI_SNOOP_FILTER:
         if(isLineInOtherProcs(addr) == NULL){
            mem_transactions++;
         }else{
            c2c_transfers++;
         }
         break;
      
      default:  // Allocating from mem
         mem_transactions++;
         break;
   }

   tag = calcTag(addr);   
   victim->setTag(tag);
   victim->setState(VALID); // will be overwritten right after   
   /** note that this cache line has been already 
      upgraded to MRU in the previous function (findLineToReplace) **/

   return victim;
}

void Cache::printStats(ulong procNum)
{ 
   printf("============ Simulation results (Cache %lu) ============\n", procNum);
   printf("01. number of reads: %lu\n", reads);
   printf("02. number of read misses: %lu\n", readMisses);
   printf("03. number of writes: %lu\n", writes);
   printf("04. number of write misses: %lu\n", writeMisses);
   printf("05. total miss rate: %.2lf%%\n", ((double) (readMisses + writeMisses) / (double) (reads + writes)) *100);
   printf("06. number of writebacks: %lu\n", writeBacks);
   printf("07. number of cache-to-cache transfers: %lu\n", c2c_transfers);
   printf("08. number of memory transactions: %lu\n", mem_transactions);
   printf("09. number of interventions: %lu\n", interventions);
   printf("10. number of invalidations: %lu\n", invalidations);
   printf("11. number of flushes: %lu\n", flush_cnt);
   printf("12. number of BusRdX: %lu\n", BusRdX_cnt);
   printf("13. number of BusUpgr: %lu\n", BusUpgr_cnt);
   if(protocol == MESI_SNOOP_FILTER){
      printf("14. number of useful snoops: %lu\n", useful_snoops_cnt);
      printf("15. number of wasted snoops: %lu\n", wasted_snoops_cnt);
      printf("16. number of filtered snoops: %lu\n", filtered_snoops_cnt);
   }
}
