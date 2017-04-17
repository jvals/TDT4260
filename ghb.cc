/*
 * Global History Buffer
 *
 * Based on:
 * Data Cache Prefetching Using a Global History Buffer
 *
 */

#include "interface.hh"

#define GHB_SIZE 256
#define IDX_SIZE 90
#define DELTA_SIZE 10
#define LOOKAHEAD 7

/* DATA STRUCTURES AND GLOBALS */
typedef struct {
  Addr PC = 0;
  Addr current = 0;
} index_table_entry;

typedef struct {
  Addr PC = 0;
  Addr next = 0;
  Addr prev = 0;
} ghb_entry;

Addr head = 0;

ghb_entry* ghb_ent;
index_table_entry* idx_ent;

ghb_entry ghb[GHB_SIZE];
index_table_entry idx[IDX_SIZE];
Addr deltas[DELTA_SIZE];

/* PROTOTYPES */
bool can_prefetch(Addr prefetch_address);

/* GLOBAL FUNCTIONS */
void prefetch_init(void) {
  /* Called before any calls to prefetch_access. */
  /* This is the place to initialize data structures. */
}

void prefetch_access(AccessStat stat) {
  head++;
  ghb_ent = &ghb[head % GHB_SIZE];
  ghb_ent->PC = stat.mem_addr;
  ghb_ent->next = 0;
  ghb_ent->prev = 0;

  idx_ent = &idx[stat.pc % IDX_SIZE];

  if (idx_ent->PC != stat.pc) {
    idx_ent->PC = stat.pc;
    idx_ent->current = head % GHB_SIZE;
  } else {
    ghb_ent->prev = idx_ent->current;
    ghb[idx_ent->current].next = head % GHB_SIZE;
    idx_ent->current = head % GHB_SIZE;
  }

  Addr prefetch_addr = stat.mem_addr;

  for (int i = 0; i < DELTA_SIZE; i++) {
    deltas[i] = ghb_ent->PC - ghb[ghb_ent->prev].PC;
    ghb_ent = &ghb[ghb_ent->prev];
  }

  for (int i = 0; i < DELTA_SIZE - 2; i++) {
    if (deltas[i + 1] == deltas[0] && deltas[i + 2] == deltas[1]) {
      for (int j = 0; j < LOOKAHEAD; j++) {
        prefetch_addr += deltas[i - (j % (i + 1)) + 2];
        if (can_prefetch(prefetch_addr)) {
          issue_prefetch(prefetch_addr);
        }
      }
      break;
    }
  }
}

void prefetch_complete(Addr addr) {
  /*
   * Called when a block requested by the prefetcher has been loaded.
   */
}

/* PRIVATE FUNCTIONS */
bool can_prefetch(Addr prefetch_address) {
  return (!in_cache(prefetch_address) && !in_mshr_queue(prefetch_address) &&
          prefetch_address <= MAX_PHYS_MEM_ADDR);
}
