#ifndef SDP_H_
#define SDP_H_

#include <stdint.h>
#include <hidapi/hidapi.h>

int sdp_write_file(hid_device *handle, const char *file_path, uint32_t address);
int sdp_error_status(hid_device *handle, uint32_t *hab_status, uint32_t *status);
int sdp_jump_address(hid_device *handle, uint32_t address);

#endif
