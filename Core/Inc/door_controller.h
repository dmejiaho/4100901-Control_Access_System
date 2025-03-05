#ifndef DOOR_CONTROLLER_H
#define DOOR_CONTROLLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opens the door temporarily (door remains open for 5 sec) */
void open_door_temp(void);

/* Opens the door permanently */
void open_door_perm(void);

/* Closes the door */
void close_door(void);

/* Checks and auto-closes the door if temporary open time has expired */
void door_controller_auto_close_check(void);

/* Returns nonzero if the door is open */
uint8_t door_is_open(void);

/* Returns nonzero if the door is set to permanent open */
uint8_t door_is_permanent(void);

#ifdef __cplusplus
}
#endif

#endif  /* DOOR_CONTROLLER_H */
