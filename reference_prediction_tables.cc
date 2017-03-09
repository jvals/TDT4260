/*
 * Reference Prediction Tables
 *
 * Based on:
 * Effective hardware-based data prefetching for high-performance
 * processors
 * (Tien-Fu Chen and Jean-Loup Baer 609-623)
 * https://www.cs.utah.edu/~rajeev/cs7810/papers/chen95.pdf
 *
 * +----+--------------+-------+-------+
 * | PC | Last Address | Delta | State |
 * +----+--------------+-------+-------+
 *
 *
 *
 * +------+  <-------------+  +--------+
 * |      |     INCORRECT     |        | <------+
 * | INIT |                   | STEADY |        |
 * |      |      CORRECT      |        | +------+
 * +------+  +------------->  +--------+ CORRECT
 *
 * +                              ^
 * | INCORRECT          CORRECT   |
 * | (UPDATE STRIDE)  +-----------+
 * v                  |
 *                    |
 * +-----------+ -----+   +------------+
 * |           | <------+ |    NO      | <------+
 * | TRANSIENT | CORRECT  | PREDICTION |        |
 * |           |          |            | +------+
 * +-----------+ +------> +------------+ INCORRECT
 *              INCORRECT              (UPDATE STRIDE)
 *           (UPDATE STRIDE)
 */

#include <string.h>
#include "interface.hh"

#define MAX_SIZE 256

/* DATA STRUCTURES AND GLOBALS */

enum state {INIT, /* Set at first entry in the RPT or  after the entry
                     experienced an incorrect prediction */
            STEADY, /* Indicates that the prediction should be table
                       for a while */
            TRANSIENT, /* Corresponds to the case when the system is
                          not sure whether the prveious prediction
                          was good or not. The new delta will be
                          obtained by subtracting the previous
                          address from the currently referenced
                          address */
            NO_PREDICTION}; /* Disables the prefetching this entry */

struct entry {
  Addr PC; /* Corresponds to the address of the Load/Store
              intruction */
  Addr last_address; /* The last address that was referenced when the
                        PC reached that intruction. */
  int delta; /* The difference between the last two addresses that
                were generated */
  enum state entry_state; /* An encoding of the past history. It
                             indicates how further prefetching should
                             be generated. */
};

struct entry *ent;
entry entry_table[MAX_SIZE] = {0};


/* PROTOTYPES */

bool correct(Addr addr);
entry* get_entry(Addr addr);
bool has_addr(Addr addr);

/* GLOBAL FUNCTIONS */

void prefetch_init(void)
{
  /* Called before any calls to prefetch_access. */
  /* This is the place to initialize data structures. */
}

void prefetch_access(AccessStat stat)
{
  ent = get_entry(stat.pc);

  if (!has_addr(stat.pc)) { /* Table miss */
    ent -> PC = stat.pc;
    ent -> last_address = stat.mem_addr;
    ent -> delta = 0;
    ent -> entry_state = INIT;
  } else { /* Table hit */
    enum state original_state = ent->entry_state;
    if (correct(stat.mem_addr)) { /* Correct */
      /* TODO: Invert this case*/
      if ((original_state == INIT) ||
          (original_state == TRANSIENT) ||
          (original_state == STEADY)) {
        ent->entry_state = STEADY;
      } else { /* original_state == NO_PREDICTION */
        ent->entry_state = TRANSIENT;
      }
    } else { /* Incorrect */
      if (original_state == INIT) {
        ent->entry_state = TRANSIENT;
      } else if (original_state == STEADY) {
        ent->entry_state = INIT;
      } else { /* original_state == TRANSIENT || NO_PREDICTION */
        ent->entry_state = NO_PREDICTION;
      }
    }
    if (original_state != STEADY || correct(stat.mem_addr)) {
      ent->delta = stat.mem_addr - ent->last_address;
      ent->last_address = stat.mem_addr;
    }
    Addr prefetch_addr = stat.mem_addr + ent->delta;
    if (ent->entry_state != NO_PREDICTION &&
        prefetch_addr <= MAX_PHYS_MEM_ADDR &&
        !in_cache(prefetch_addr) &&
        !in_mshr_queue(prefetch_addr)) {
      issue_prefetch(prefetch_addr);
    }
  }
}

void prefetch_complete(Addr addr) {
  /*
   * Called when a block requested by the prefetcher has been loaded.
   */
}

/* PRIVATE FUNCTIONS */

bool correct(Addr current_addr) {
  return current_addr == ent->last_address + ent->delta;
}

entry* get_entry(Addr addr) {
  return &entry_table[addr % MAX_SIZE];
}

bool has_addr(Addr addr) {
  return addr == entry_table[addr % MAX_SIZE].PC;
}
