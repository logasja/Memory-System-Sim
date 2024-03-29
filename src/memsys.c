#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "memsys.h"

#define PAGE_SIZE 4096

//---- Cache Latencies  ------

#define DCACHE_HIT_LATENCY   1
#define ICACHE_HIT_LATENCY   1
#define L2CACHE_HIT_LATENCY  10

extern MODE   SIM_MODE;
extern uns64  CACHE_LINESIZE;
extern uns64  REPL_POLICY;

extern uns64  DCACHE_SIZE;
extern uns64  DCACHE_ASSOC;
extern uns64  ICACHE_SIZE;
extern uns64  ICACHE_ASSOC;
extern uns64  L2CACHE_SIZE;
extern uns64  L2CACHE_ASSOC;
extern uns64  L2CACHE_REPL;
extern uns64  NUM_CORES;

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////


Memsys *memsys_new(void)
{
    Memsys *sys = (Memsys *) calloc (1, sizeof (Memsys));

      if(SIM_MODE==SIM_MODE_A){
        sys->dcache = cache_new(DCACHE_SIZE, DCACHE_ASSOC, CACHE_LINESIZE, REPL_POLICY);
      }

      if(SIM_MODE==SIM_MODE_B){
        sys->dcache = cache_new(DCACHE_SIZE, DCACHE_ASSOC, CACHE_LINESIZE, REPL_POLICY);
        sys->icache = cache_new(ICACHE_SIZE, ICACHE_ASSOC, CACHE_LINESIZE, REPL_POLICY);
        sys->l2cache = cache_new(L2CACHE_SIZE, L2CACHE_ASSOC, CACHE_LINESIZE, REPL_POLICY);
        sys->dram    = dram_new();
      }

      if(SIM_MODE==SIM_MODE_C){
        sys->dcache = cache_new(DCACHE_SIZE, DCACHE_ASSOC, CACHE_LINESIZE, REPL_POLICY);
        sys->icache = cache_new(ICACHE_SIZE, ICACHE_ASSOC, CACHE_LINESIZE, REPL_POLICY);
        sys->l2cache = cache_new(L2CACHE_SIZE, L2CACHE_ASSOC, CACHE_LINESIZE, REPL_POLICY);
        sys->dram    = dram_new();
      }

      if( (SIM_MODE==SIM_MODE_D) || (SIM_MODE==SIM_MODE_E) || (SIM_MODE==SIM_MODE_F) ) {
        sys->l2cache = cache_new(L2CACHE_SIZE, L2CACHE_ASSOC, CACHE_LINESIZE, L2CACHE_REPL);
        sys->dram    = dram_new();
        uns ii;
        for(ii=0; ii<NUM_CORES; ii++){
          sys->dcache_coreid[ii] = cache_new(DCACHE_SIZE, DCACHE_ASSOC, CACHE_LINESIZE, REPL_POLICY);
          sys->icache_coreid[ii] = cache_new(ICACHE_SIZE, ICACHE_ASSOC, CACHE_LINESIZE, REPL_POLICY);
        }
      }

      return sys;
}


////////////////////////////////////////////////////////////////////
// This function takes an ifetch/ldst access and returns the delay
////////////////////////////////////////////////////////////////////

uns64 memsys_access(Memsys *sys, Addr addr, Access_Type type, uns core_id)
{
    uns delay=0;


    // all cache transactions happen at line granularity, so get lineaddr
    Addr lineaddr=addr/CACHE_LINESIZE;

    if(SIM_MODE==SIM_MODE_A){
        delay = memsys_access_modeA(sys,lineaddr,type, core_id);
    }

    if((SIM_MODE==SIM_MODE_B)||(SIM_MODE==SIM_MODE_C)){
        delay = memsys_access_modeBC(sys,lineaddr,type, core_id);
    }

    if((SIM_MODE==SIM_MODE_D)||(SIM_MODE==SIM_MODE_E) ||(SIM_MODE==SIM_MODE_F)  ){
        delay = memsys_access_modeDEF(sys,lineaddr,type, core_id);
    }

    //update the stats
    if(type==ACCESS_TYPE_IFETCH){
        sys->stat_ifetch_access++;
        sys->stat_ifetch_delay+=delay;
    }

    if(type==ACCESS_TYPE_LOAD){
        sys->stat_load_access++;
        sys->stat_load_delay+=delay;
    }

    if(type==ACCESS_TYPE_STORE){
        sys->stat_store_access++;
        sys->stat_store_delay+=delay;
    }


    return delay;
}



////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void memsys_print_stats(Memsys *sys)
{
  char header[256];
  sprintf(header, "MEMSYS");

  double ifetch_delay_avg=0;
  double load_delay_avg=0;
  double store_delay_avg=0;

  if(sys->stat_ifetch_access){
    ifetch_delay_avg = (double)(sys->stat_ifetch_delay)/(double)(sys->stat_ifetch_access);
  }

  if(sys->stat_load_access){
    load_delay_avg = (double)(sys->stat_load_delay)/(double)(sys->stat_load_access);
  }

  if(sys->stat_store_access){
    store_delay_avg = (double)(sys->stat_store_delay)/(double)(sys->stat_store_access);
  }


  printf("\n");
  printf("\n%s_IFETCH_ACCESS  \t\t : %10llu",  header, sys->stat_ifetch_access);
  printf("\n%s_LOAD_ACCESS    \t\t : %10llu",  header, sys->stat_load_access);
  printf("\n%s_STORE_ACCESS   \t\t : %10llu",  header, sys->stat_store_access);
  printf("\n%s_IFETCH_AVGDELAY\t\t : %10.3f",  header, ifetch_delay_avg);
  printf("\n%s_LOAD_AVGDELAY  \t\t : %10.3f",  header, load_delay_avg);
  printf("\n%s_STORE_AVGDELAY \t\t : %10.3f",  header, store_delay_avg);
  printf("\n");

   if(SIM_MODE==SIM_MODE_A){
    cache_print_stats(sys->dcache, "DCACHE");
  }

  if((SIM_MODE==SIM_MODE_B)||(SIM_MODE==SIM_MODE_C)){
    cache_print_stats(sys->icache, "ICACHE");
    cache_print_stats(sys->dcache, "DCACHE");
    cache_print_stats(sys->l2cache, "L2CACHE");
    dram_print_stats(sys->dram);
  }

  if((SIM_MODE==SIM_MODE_D)||(SIM_MODE==SIM_MODE_E)||(SIM_MODE==SIM_MODE_F) ){
    assert(NUM_CORES==2); //Hardcoded
    cache_print_stats(sys->icache_coreid[0], "ICACHE_0");
    cache_print_stats(sys->dcache_coreid[0], "DCACHE_0");
    cache_print_stats(sys->icache_coreid[1], "ICACHE_1");
    cache_print_stats(sys->dcache_coreid[1], "DCACHE_1");
    cache_print_stats(sys->l2cache, "L2CACHE");
    dram_print_stats(sys->dram);

  }

}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

uns64 memsys_access_modeA(Memsys *sys, Addr lineaddr, Access_Type type, uns core_id){
  Flag needs_dcache_access=FALSE;
  Flag is_write=FALSE;

  if(type == ACCESS_TYPE_IFETCH){
    // no icache in this mode
  }

  if(type == ACCESS_TYPE_LOAD){
    needs_dcache_access=TRUE;
    is_write=FALSE;
  }

  if(type == ACCESS_TYPE_STORE){
    needs_dcache_access=TRUE;
    is_write=TRUE;
  }

  if(needs_dcache_access){
    Flag outcome=cache_access(sys->dcache, lineaddr, is_write,core_id);
    if(outcome==MISS){
      cache_install(sys->dcache, lineaddr, is_write,core_id);
    }
  }

  // timing is not simulated in Part A
  return 0;
}

/////////////////////////////////////////////////////////////////////
// This function converts virtual page number (VPN) to physical frame
// number (PFN).  Note, you will need additional operations to obtain
// VPN from lineaddr and to get physical lineaddr using PFN.
/////////////////////////////////////////////////////////////////////

uns64 memsys_convert_vpn_to_pfn(Memsys *sys, uns64 vpn, uns core_id){
    uns64 tail = vpn & 0x000fffff;
    uns64 head = vpn >> 20;
    uns64 pfn  = tail + (core_id << 21) + (head << 21);
    assert(NUM_CORES==2); //We don't support more than two cores yet
    return pfn;
}

////////////////////////////////////////////////////////////////////
// --------------- DO NOT CHANGE THE CODE ABOVE THIS LINE ----------
////////////////////////////////////////////////////////////////////

uns64 memsys_access_modeBC(Memsys *sys, Addr lineaddr, Access_Type type,uns core_id){
    uns64 delay=0;
    Flag is_write = FALSE;
    Cache* use_cache = NULL;
    Flag result;

    switch(type) {
        case ACCESS_TYPE_IFETCH:
            use_cache = sys->icache;
            is_write = FALSE;
            delay = ICACHE_HIT_LATENCY;
            break;
        case ACCESS_TYPE_LOAD:
            use_cache = sys->dcache;
            is_write = FALSE;
            delay = DCACHE_HIT_LATENCY;
            break;
        case ACCESS_TYPE_STORE:
            use_cache = sys->dcache;
            is_write = TRUE;
            delay = DCACHE_HIT_LATENCY;
            break;
    }
    result = cache_access(use_cache, lineaddr, is_write, core_id);
    if(result == MISS) {
        delay += memsys_L2_access(sys, lineaddr, FALSE, core_id);
        cache_install(use_cache, lineaddr, is_write, core_id);
        Cache_Line* line = &use_cache->last_evicted_line;
        if(line->valid && line->dirty)
            memsys_L2_access(sys, line->tag, TRUE, core_id);
    }
    return delay;
}

/////////////////////////////////////////////////////////////////////
// For Mode D/E/F you will use per-core ICACHE and DCACHE
// ----- YOU NEED TO WRITE THIS FUNCTION AND UPDATE DELAY ----------
/////////////////////////////////////////////////////////////////////


uns64 memsys_access_modeDEF(Memsys *sys, Addr v_lineaddr, Access_Type type,uns core_id){
    uns64 delay=0;
    Addr p_lineaddr=0;
    Flag is_write = FALSE;
    Cache* use_cache = NULL;
    Flag result;

    // Remove page offset from lineaddress
    Addr v_page_num = v_lineaddr / (PAGE_SIZE / CACHE_LINESIZE);
    Addr v_page_offset = v_lineaddr % (PAGE_SIZE / CACHE_LINESIZE);

    // Decode frame number
    Addr p_frame_num = memsys_convert_vpn_to_pfn(sys, v_page_num, core_id);

    // Offset will be invariant between translations
    p_lineaddr = (p_frame_num * (PAGE_SIZE / CACHE_LINESIZE)) + v_page_offset;

    // TODO: First convert lineaddr from virtual (v) to physical (p) using the
    // function memsys_convert_vpn_to_pfn. Page size is defined to be 4KB.
    // NOTE: VPN_to_PFN operates at page granularity and returns page addr

    switch(type) {
        case ACCESS_TYPE_IFETCH:
            use_cache = sys->icache_coreid[core_id];
            is_write = FALSE;
            delay = ICACHE_HIT_LATENCY;
            break;
        case ACCESS_TYPE_LOAD:
            use_cache = sys->dcache_coreid[core_id];
            is_write = FALSE;
            delay = DCACHE_HIT_LATENCY;
            break;
        case ACCESS_TYPE_STORE:
            use_cache = sys->dcache_coreid[core_id];
            is_write = TRUE;
            delay = DCACHE_HIT_LATENCY;
            break;
    }
    // Access per-core caches
    result = cache_access(use_cache, p_lineaddr, is_write, core_id);
    // Only for simulation
    if (result == MISS) {
        // Access shared L2
        delay += memsys_L2_access(sys, p_lineaddr, FALSE, core_id);
        // Install into the cache of core requesting
        cache_install(use_cache, p_lineaddr, is_write, core_id);
        Cache_Line* line = &use_cache->last_evicted_line;
        if(line->valid && line->dirty) {
            // Similarly shared
            memsys_L2_access(sys, line->tag, TRUE, line->core_id);
            line->valid = FALSE;
        }
    }
    return delay;
  }

/////////////////////////////////////////////////////////////////////
// This function is called on ICACHE miss, DCACHE miss, DCACHE writeback
// ----- YOU NEED TO WRITE THIS FUNCTION AND UPDATE DELAY ----------
/////////////////////////////////////////////////////////////////////

uns64   memsys_L2_access(Memsys *sys, Addr lineaddr, Flag is_writeback, uns core_id){
    uns64 delay = L2CACHE_HIT_LATENCY;

    Flag result = cache_access(sys->l2cache, lineaddr, is_writeback, core_id);
    //To get the delay of L2 MISS, you must use the dram_access() function
    //To perform writebacks to memory, you must use the dram_access() function
    //This will help us track your memory reads and memory writes
    if(result == MISS) {
        delay += dram_access(sys->dram, lineaddr, FALSE);
        cache_install(sys->l2cache, lineaddr, is_writeback, core_id);
        Cache_Line* line = &sys->l2cache->last_evicted_line;
        if(line->valid && line->dirty) {
            dram_access(sys->dram, line->tag, TRUE);
            line->valid = FALSE;
        }
    }
    return delay;
}
