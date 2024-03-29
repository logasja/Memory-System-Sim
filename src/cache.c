#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "cache.h"


extern uns64 SWP_CORE0_WAYS; // Input Way partitions for Core 0       
extern uns64 cycle; // You can use this as timestamp for LRU

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE INIT FUNCTION -----------
////////////////////////////////////////////////////////////////////

Cache  *cache_new(uns64 size, uns64 assoc, uns64 linesize, uns64 repl_policy){

   Cache *c = (Cache *) calloc (1, sizeof (Cache));
   c->num_ways = assoc;
   c->repl_policy = repl_policy;

   if(c->num_ways > MAX_WAYS){
     printf("Change MAX_WAYS in cache.h to support %llu ways\n", c->num_ways);
     exit(-1);
   }

   // determine num sets, and init the cache
   c->num_sets = size/(linesize*assoc);
   c->sets  = (Cache_Set *) calloc (c->num_sets, sizeof(Cache_Set));

   return c;
}

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE PRINT STATS FUNCTION -----------
////////////////////////////////////////////////////////////////////

void    cache_print_stats    (Cache *c, char *header){
  double read_mr =0;
  double write_mr =0;

  if(c->stat_read_access){
    read_mr=(double)(c->stat_read_miss)/(double)(c->stat_read_access);
  }

  if(c->stat_write_access){
    write_mr=(double)(c->stat_write_miss)/(double)(c->stat_write_access);
  }

  printf("\n%s_READ_ACCESS    \t\t : %10llu", header, c->stat_read_access);
  printf("\n%s_WRITE_ACCESS   \t\t : %10llu", header, c->stat_write_access);
  printf("\n%s_READ_MISS      \t\t : %10llu", header, c->stat_read_miss);
  printf("\n%s_WRITE_MISS     \t\t : %10llu", header, c->stat_write_miss);
  printf("\n%s_READ_MISSPERC  \t\t : %10.3f", header, 100*read_mr);
  printf("\n%s_WRITE_MISSPERC \t\t : %10.3f", header, 100*write_mr);
  printf("\n%s_DIRTY_EVICTS   \t\t : %10llu", header, c->stat_dirty_evicts);

  printf("\n");
}



////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Return HIT if access hits in the cache, MISS otherwise 
// Also if is_write is TRUE, then mark the resident line as dirty
// Update appropriate stats
////////////////////////////////////////////////////////////////////

Flag cache_access(Cache *c, Addr lineaddr, uns is_write, uns core_id){
    Flag outcome=MISS;

    int set = lineaddr % c->num_sets;
    lineaddr /= c->num_sets;
    // Your Code Goes Here
    Cache_Line* line;
    for(uns i = 0; i < c->num_ways; i++) {
        line = &c->sets[set].line[i];
        if (!line->valid) continue;
        if(line->tag == lineaddr && line->core_id == core_id){
            outcome = HIT;
            break;
        }
    }

    // If a line was found, check if write and mark as dirty
    // Set line last access time
    if(outcome == HIT){
        if (is_write == TRUE) {
            line->dirty = TRUE;
            ++c->stat_write_access;
        } else
            ++c->stat_read_access;
        line->last_access_time = cycle;
    } else {
        if (is_write == TRUE) {
            ++c->stat_write_miss;
            ++c->stat_write_access;
        } else {
            ++c->stat_read_miss;
            ++c->stat_read_access;
        }
    }

    return outcome;
}

////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Install the line: determine victim using repl policy (LRU/RAND)
// copy victim into last_evicted_line for tracking writebacks
////////////////////////////////////////////////////////////////////

void cache_install(Cache *c, Addr lineaddr, uns is_write, uns core_id){
    int set = lineaddr % c->num_sets;
    lineaddr /= c->num_sets;
    // Find victim using cache_find_victim
    uns victim = cache_find_victim(c, set, core_id); 
    // Initialize the evicted entry
    c->last_evicted_line = c->sets[set].line[victim];
    c->last_evicted_line.tag = (c->last_evicted_line.tag * c->num_sets) + set;
    // Initialize the victime entry
    Cache_Line newLine;
    newLine.core_id = core_id;
    newLine.dirty = is_write;
    newLine.tag = lineaddr;
    newLine.valid = TRUE;
    newLine.last_access_time = cycle;
    c->sets[set].line[victim] = newLine;
}

////////////////////////////////////////////////////////////////////
// You may find it useful to split victim selection from install
////////////////////////////////////////////////////////////////////


uns cache_find_victim(Cache *c, uns set_index, uns core_id){
    uns victim=0;
    uns minAccessTime = 0xFFFFFFFF;

    // If there is space in the cache, don't need to
    // replace
    Cache_Line* line;
    for(uns i = 0; i < c->num_ways; i++) {
        line = &c->sets[set_index].line[i];    
        if(!line->valid)
            return i;
    }

    // Get victim based on policy
    switch(c->repl_policy){
        case 0:    // LRU
            for(uns i = 0; i < c->num_ways; i++) {
                line = &c->sets[set_index].line[i];
                if(line->last_access_time < minAccessTime) {
                    victim = i;
                    minAccessTime = line->last_access_time;
                }
            }
            break;
        case 1:    // RAND
            victim = rand() % c->num_ways;
            break;
        case 2: ;    // Static Way Partitioning
            uns64 core0_entries = 0;
            uns64 core1_entries = 0;
            // Count entries for each way
            for(uns i = 0; i < c->num_ways; i++) {
                uns id = c->sets[set_index].line[i].core_id;
                if(id == 0) { ++core0_entries; }
                else { ++core1_entries; }
            }
            // If one has too many allocated, choose victim from that set
            // Calculate which core will have entry removed
            uns victim_core = core_id;
            uns64 SWP_CORE1_WAYS = c->num_ways - SWP_CORE0_WAYS;
            // If a core has more entries than its quotia, choose victim from its set
            if(core0_entries < SWP_CORE0_WAYS){
                victim_core = 1;
            } else if(core1_entries < SWP_CORE1_WAYS) {
                victim_core = 0;
            }
            // LRU replacement
            for(uns i = 0; i < c->num_ways; i++) {
                line = &c->sets[set_index].line[i];
                if(line->last_access_time < minAccessTime && line->core_id == victim_core) {
                    victim = i;
                    minAccessTime = line->last_access_time;
                }
            }
            break;
        default:
            break;
    }

    // Update stats
    if(c->sets[set_index].line[victim].dirty)
        ++c->stat_dirty_evicts;
    return victim;
}

