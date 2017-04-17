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
#define LOOK_AHEAD_LIMIT 8

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
  int times; /* Keep track of how many iterations the LA-PC is ahead
                of the PC */
};

struct entry *ent;
entry entry_table[MAX_SIZE] = {0};


/* PROTOTYPES */

bool can_prefetch(Addr addr);
void times_increment(void);
void times_decrement(void);
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
    ent -> times = 1;
  } else { /* Table hit */
    enum state original_state = ent->entry_state;
    if (correct(stat.mem_addr)) { /* Correct */
      /* TODO: Invert this case*/
      times_increment();
      if ((original_state == INIT) ||
          (original_state == TRANSIENT) ||
          (original_state == STEADY)) {
        ent->entry_state = STEADY;
      } else { /* original_state == NO_PREDICTION */
        ent->entry_state = TRANSIENT;
      }
    } else { /* Incorrect */
      times_decrement();
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

    for (int i = 1; i <= ent->times; i++) {
      Addr prefetch_addr = stat.mem_addr + ent->delta * i;
      if (can_prefetch(prefetch_addr)) {
	issue_prefetch(prefetch_addr);
      }
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
  return (ent->entry_state != NO_PREDICTION &&
          !in_cache(prefetch_address) &&
          !in_mshr_queue(prefetch_address) &&
          prefetch_address <= MAX_PHYS_MEM_ADDR);
}

void times_increment() {
  if (ent->times < LOOK_AHEAD_LIMIT) {
    ent->times++;
  }
}

void times_decrement() {
  if (ent->times > 1) {
    ent->times--;
  }
}

bool correct(Addr current_addr) {
  return current_addr == ent->last_address + ent->delta;
}

entry* get_entry(Addr addr) {
  return &entry_table[addr % MAX_SIZE];
}

bool has_addr(Addr addr) {
  return addr == entry_table[addr % MAX_SIZE].PC;
}
