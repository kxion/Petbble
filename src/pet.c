#include "pet.h"
#include "pebble.h"

#define MAX_STAT 4

#define ONE_SECOND  1
#define ONE_MINUTE  (ONE_SECOND * 60)
#define ONE_HOUR    (ONE_MINUTE * 60)
#define ONE_DAY     (ONE_HOUR * 24)


int pet_load_state(Pet *p) {
    for (int i = 0; i < NUM_PET_FIELDS; i++) {
        if (persist_exists(i)) {
            p->fields[i] = persist_read_int(i);
        } else {
            return 0;
        }
    }
    return 1;
}

void pet_save_state(Pet *p) {
    p->fields[LAST_OPEN_KEY] = time(NULL);
    for (int i = 0; i < NUM_PET_FIELDS; i++) {
        persist_write_int(i, p->fields[i]);
    }
}

void pet_die(Pet *p) {
    // TODO: unimplemented
}

// Returns 1 if key is a stat from 0 - 3, and 0 otherwise.
static int counting_stat(int i) {
    switch (i) {
        case HEALTH_KEY:
        case HAPPINESS_KEY:
        case HUNGER_KEY:
        case DISCIPLINE_KEY:
        case ENERGY_KEY:
            return 1;
            break;
        default:
            return 0;
  }
}

// Returns a rand number between low and high, inclusive on the low, exclusive on high.
static int randRange(int low, int high) {
    if (high <= low) {
        return 0;
    }
    return (rand() % (high - low) + low);
}

// Attempts to increase stat by 1.  If it is successful, returns 1.  Otherwise returns 0.
static int increment_stat(Pet *p, int stat) {
    if (stat < MAX_STAT) {
        p->fields[stat] += 1;
        return 1;
    }
    return 0;
}

// Attempts to decrease stat by 1.  If it is successful, returns 1.  Otherwise returns 0.
static int decrement_stat(Pet *p, int stat) {
    if (stat > 0) {
        p->fields[stat] -= 1;
        return 1;
    }
    return 0;
}

void pet_new(Pet *p) {
    p->fields[CURRENT_STAGE_KEY] = 0;
    p->fields[HEALTH_KEY] = 0;          // TODO change to 3
    p->fields[HAPPINESS_KEY] = 1;       // TODO change to 3
    p->fields[HUNGER_KEY] = 2;          // TODO change to 3
    p->fields[DISCIPLINE_KEY] = 3;      // TODO change to 3
    p->fields[WEIGHT_KEY] = randRange(5, 10);
    p->fields[ENERGY_KEY] = 3;
    p->fields[SICK_KEY] = 0;
    p->fields[TOTAL_AGE_KEY] = time(NULL);
    p->fields[CURRENT_STAGE_AGE_KEY] = time(NULL);
    p->fields[LAST_OPEN_KEY] = time(NULL);
    p->fields[SLEEPING_KEY] = 0;
    p->fields[POOP_KEY] = 0;
    for (int i = 0; i < NUM_PET_FIELDS; i++) {
        persist_delete(i);
    }
}

// Pet Actions

void pet_feed(Pet *p) {
    p->fields[WEIGHT_KEY] = p->fields[WEIGHT_KEY] + 1;
    // Attempt to increase satiation
    if (increment_stat(p, HUNGER_KEY)) {
        return;
    // If already full, then decrease health
    } else if (decrement_stat(p, HEALTH_KEY)) {
        return;
    } else {
        // Dies from over feeding? (0 health, full belly.)
        pet_die(p);
    }
}

int pet_attempt_play(Pet *p) {
    return decrement_stat(p, ENERGY_KEY);
}

void pet_play(Pet *p, int score) {
    // Lose 1 pound of weight for each point
    for (int i = 0; i < score; i++) {
        decrement_stat(p, WEIGHT_KEY);
    }
    for (int i = 0; i < score / 10; i++) {
        increment_stat(p, HAPPINESS_KEY);
        increment_stat(p, HEALTH_KEY);
    }
}

void pet_heal(Pet *p) {
    if (p->fields[SICK_KEY]) {
        p->fields[SICK_KEY] = 0;
    } else {
        decrement_stat(p, DISCIPLINE_KEY);
    }
}

void pet_sleep(Pet *p) {
    p->fields[SLEEPING_KEY] = 1;
}

void pet_wake_up(Pet *p) {
    p->fields[SLEEPING_KEY] = 1;
}

void pet_bath(Pet *p) {
    if (p->fields[POOP_KEY] > 0) {
        p->fields[POOP_KEY] = 0;
        increment_stat(p, HAPPINESS_KEY);
    } else {
        decrement_stat(p, HAPPINESS_KEY);
    }
}

void pet_discipline(Pet *p) {
    if (p->fields[POOP_KEY] == 0) {
        decrement_stat(p, DISCIPLINE_KEY);
    } else if (p->fields[POOP_KEY] == 1) {
        increment_stat(p, DISCIPLINE_KEY);
        // Change key to 2 so that you can only discipline once per poop.
        p->fields[POOP_KEY] = 2;
    }
}

// Tamogatch starts at age 1.  Ages at 1 year per day.
int pet_calculate_age(Pet *p) {
    int time_second = time(NULL) - p->fields[TOTAL_AGE_KEY];
    return 1 + time_second / ONE_DAY;
}

static void update_stats(Pet *p, int i) {
    // Approximately every i mintes, stats decrease by 1.
    if (rand() % i == 0) {
        decrement_stat(p, HEALTH_KEY);
    }
    if (rand() % i == 0) {
        decrement_stat(p, HAPPINESS_KEY);
    }
    if (rand() % i == 0) {
        decrement_stat(p, HUNGER_KEY);
    }
    if (rand() % i == 0) {
        decrement_stat(p, DISCIPLINE_KEY);
    }
    if (rand() % i == 0) {
        decrement_stat(p, WEIGHT_KEY);
    }
    // Increase energy while asleep
    if (p->fields[SLEEPING_KEY] && (rand() % i == 0)) {
        increment_stat(p, ENERGY_KEY);
    }
    // Hungry and unhealthy leads to greater chance of getting sick.
    if (rand() % (5 * i * (p->fields[HEALTH_KEY] + p->fields[HUNGER_KEY]))) {
        p->fields[SICK_KEY] = 1;
    }
}

// Periodically called to see if conditions are met for various statuses
// @return 1 if any changes were made; 0 otherwise.
int pet_check_status(Pet *p) {
    int modify = 0;
    switch (p->fields[CURRENT_STAGE_KEY]) {
        case EGG_STAGE:
            if (time(NULL) - p->fields[CURRENT_STAGE_AGE_KEY] < ONE_SECOND * 10) {
                break;
            }
            p->fields[CURRENT_STAGE_KEY] = BABITCHI_STAGE;
            p->fields[HEALTH_KEY] = 4;
            p->fields[HAPPINESS_KEY] = 3;
            p->fields[HUNGER_KEY] = 3;
            p->fields[DISCIPLINE_KEY] = 2;
            // WEIGHT purposefully not touched.
            p->fields[CURRENT_STAGE_AGE_KEY] = time(NULL);
            modify = 1;
            break;
        case BABITCHI_STAGE:
            update_stats(p, 10);
            for (int i = 0; i < NUM_PET_FIELDS; i++) {
                // If one of the stats required to evolve is not maximum, then return.
                if ((time(NULL) - p->fields[CURRENT_STAGE_AGE_KEY] < ONE_DAY) || 
                   (counting_stat(i) && (p->fields[i] < MAX_STAT)) ||
                    p->fields[SICK_KEY]) {
                    break;
                }
            }
            // If all stats are max, then we can evolve.
            p->fields[CURRENT_STAGE_KEY] = KUCHIT_STAGE;
            p->fields[HEALTH_KEY] = 3;
            p->fields[HAPPINESS_KEY] = 2;
            p->fields[HUNGER_KEY] = 1;
            p->fields[DISCIPLINE_KEY] = 1;
            p->fields[WEIGHT_KEY] += 5;
            p->fields[CURRENT_STAGE_AGE_KEY] = time(NULL);
            modify = 1;
            break;
        case KUCHIT_STAGE:
            update_stats(p, 10);
            for (int i = 0; i < NUM_PET_FIELDS; i++) {
                // If one of the stats required to evolve is not maximum, then return.
                if ((time(NULL) - p->fields[CURRENT_STAGE_AGE_KEY] < 2 * ONE_DAY) || 
                   (counting_stat(i) && (p->fields[i] < MAX_STAT)) ||
                    p->fields[SICK_KEY]) {
                    break;
                }
            }
            p->fields[CURRENT_STAGE_KEY] = RETURN_STAGE;
            break;
    }
    return modify;
}


