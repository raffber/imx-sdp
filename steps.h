#ifndef STEPS_H_
#define STEPS_H_

#include <hidapi/hidapi.h>

struct sdp_step_;
typedef struct sdp_step_ sdp_step;

sdp_step *sdp_parse_step(char *s);
int sdp_execute_steps(hid_device *handle, sdp_step *steo);
sdp_step *sdp_next_step(sdp_step *step);
void sdp_set_next_step(sdp_step *step, sdp_step *next);

#endif
