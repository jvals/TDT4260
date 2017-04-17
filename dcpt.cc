/*
 * Delta Correlation Prediction Tables
 *
 * Based on: 
 *
 * +----+-------------+---------------+---------+---------+----
 * | PC | Last Adress | Last Prefetch | Delta 1 | Delta 2 | ...
 * +----+-------------+---------------+---------+---------+----
 * ---+---------+---------------+
 * ...| Delta N | Delta Pointer |
 * ---+---------+---------------+
 *
 */


#include "interface.hh"
#include <math.h>
#define DELTA_COUNT 19
#define ENTRY_COUNT 98

typedef int16_t Delta; 

struct entry {
    Addr PC;                //adress of instuction
    Addr last_address;  //last address issued by PC
    Addr last_prefetch;
    Delta deltas[DELTA_COUNT];
    Delta* delta;         //Pointer to "First" element in deltas
};

typedef struct prefetch_pair {
    Delta first;
    Delta second;
}prefetch_pair;

struct entry *ent;
struct entry entry_table[ENTRY_COUNT];
Addr candidates[DELTA_COUNT];
Addr* cand = &candidates[0];
bool has_addr(Addr addr);
entry* get_entry(AccessStat stat);
Addr* prefetch_candidates(Delta* deltas, Delta* delta, Addr* candidates);

void prefetch_init(void)
{
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */

    //DPRINTF(HWPrefetch, "Initialized sequential-on-access prefetcher\n");
}

void prefetch_access(AccessStat stat)
{
    //fetch entry from table
    ent = get_entry(stat);
    if (ent->PC != stat.pc && stat.miss){
        /* If corresponding pc not in table
         * Initialize and add to entry table
         */
        ent->PC = stat.pc;
        ent->last_address = stat.mem_addr;
        ent->last_prefetch = 0; 
        /* Deltas set to zero for new etry */
        for (int i = 0; i < DELTA_COUNT; i++){
            ent->deltas[i] = 0;
        }
        ent->delta = &ent->deltas[0]; //delta points to first delta in list
        //entry_table[stat.pc % ENTRY_COUNT] = *ent; //Is this neccesary? probably not
    }
    else if (stat.miss){
        //A cache miss occured
        if (ent->last_address != stat.mem_addr) {
            //If the last address is same as accessed do not update deltas
            if (ent->delta > &ent->deltas[0]){
                if (ent->delta == &ent->deltas[DELTA_COUNT]){
                    //Move delta head
                    ent->delta = &ent->deltas[0];
                }
                else {ent->delta++;}
            }
            //check for too large deltas
            if ((ent->last_address - stat.mem_addr) > pow(2,16) - 1 || ent-> last_address - stat.mem_addr < -pow(2,16)){
                *ent->delta = 0;
            }
            else {
                //Set value of delta head to new delta
                *ent->delta = ent->last_address - stat.mem_addr;
            }
            cand = prefetch_candidates(ent->deltas,ent->delta,cand);
            for (int i = 0; i < DELTA_COUNT; i++){
                if (candidates[i] != 0){
                   if (in_cache(candidates[i])){
                       candidates[i] = 0;
                        continue;
                   }
                   else if (in_mshr_queue(candidates[i])) {
                       candidates[i] = 0;
                        continue;
                   }
                   else if (current_queue_size()==MAX_QUEUE_SIZE){
                       candidates[i] = 0;
                        continue;
                   }
                   else {
                       /*
                       if (candidates[i] >= MAX_PHYS_MEM_ADDR){
                            continue;
                       }
                       */
                        issue_prefetch(candidates[i]);
                        ent->last_prefetch = candidates[i];
                       candidates[i] = 0;
                   }
                }
            } 
        } 
    }
}

void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     * TODO: update the last prefetch address for the corresponding entry
     */

}

bool has_addr(Addr addr) {
    bool found = false;
    for (int i = 0; i < ENTRY_COUNT; i++){
        if (addr == entry_table[i].PC){
            found = true;
        }
    }
    return found;
}

entry* get_entry(AccessStat stat){
    return &entry_table[stat.pc % ENTRY_COUNT];
}

Addr* prefetch_candidates(Delta* deltas, Delta* delta, Addr* candidates){
    int indices[DELTA_COUNT];
    int indice = delta - deltas;
    //create an array of the indices of deltas for easy transverse
    for (int i = 0; i < DELTA_COUNT; i++) {
        if (indice == 0) {
            indices[i] = indice;
            indice = DELTA_COUNT-1; 
        }
        else {
            indices[i] = indice;
            indice--;
        }
    }
    prefetch_pair firstpair;
    firstpair.first = deltas[indices[0]];
    firstpair.second = deltas[indices[1]];
    int atindex = 0;
    bool found_first = false; 
    for (int i = 1; i < DELTA_COUNT-1; i++){
        prefetch_pair compare;
        compare.first = deltas[indices[i]];
        compare.second = deltas[indices[i+1]];
        if (compare.first == firstpair.first && compare.second == firstpair.second){
            found_first = true;
        }
        if(found_first){
           for (;i>0;i--){
               Addr new_candidate;
               if (!atindex){
                    new_candidate = ent->last_address+deltas[indices[i-1]];
               }
               else {
                    new_candidate = candidates[atindex-1]+deltas[indices[i-1]];
               }
               //Addr new_candidate = 0;
               if (new_candidate == ent->last_prefetch){
                    for (int i = 0;i<=atindex;i++){
                        candidates[i]=0;
                    }
                    atindex = 0;
                    continue;
               }
               candidates[atindex] = new_candidate;
               atindex++;
           }
           return candidates;
        }
    }
    return candidates;
}
