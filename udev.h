#ifndef UDEV_H_
#define UDEV_H_

#include <stdint.h>
#include <stdbool.h>

struct sdp_udev_;
typedef struct sdp_udev_ sdp_udev;

sdp_udev *sdp_udev_init();
void sdp_udev_free(sdp_udev *udev);
char *sdp_udev_wait(sdp_udev *udev, uint16_t vid, uint16_t pid, const char *usb_path, int timeout);
bool sdp_udev_matching_usb_path(sdp_udev *udev, const char *device_path, const char *usb_path);

#endif
