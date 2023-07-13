#include "stages.h"
#include "config.h"
#include "sdp.h"
#include "steps.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WITH_UDEV
#include "udev.h"
#else
#include <unistd.h>
#endif

struct stage
{
    uint16_t usb_vid;
    uint16_t usb_pid;
    sdp_step *steps;
};

struct sdp_stages_
{
    int count;
    struct stage stages[0];
};

static int parse_stage(char *const s, struct stage *stage)
{
    char *saveptr = NULL;
    char *tok = strtok_r(s, ",", &saveptr);
    if (!tok)
    {
        fprintf(stderr, "ERROR: Stage \"%s\" invalid\n", s);
        return 1;
    }

    unsigned int vid, pid;
    int conversions = sscanf(tok, "%04x:%04x", &vid, &pid);
    if (conversions != 2)
    {
        fprintf(stderr, "ERROR: Stage didn't contain USB VID/PID");
        if (errno != 0)
            fprintf(stderr, ": %s\n", strerror(errno));
        else
            fputc('\n', stderr);
        return 1;
    }

    stage->usb_vid = vid;
    stage->usb_pid = pid;

    sdp_step *last_step;
    while ((tok = strtok_r(NULL, ",", &saveptr)))
    {
        sdp_step *step = sdp_parse_step(tok);
        if (!step)
        {
            fprintf(stderr, "ERROR: Failed to parse step\n");
            return 1;
        }

        if (!stage->steps)
            stage->steps = step;
        else
            sdp_set_next_step(last_step, step);
        last_step = step;
    }

    return 0;
}

sdp_stages *sdp_parse_stages(int count, char *s[])
{
    sdp_stages *stages = calloc(1, sizeof(sdp_stages) + count * sizeof(struct stage));
    if (!stages)
    {
        fprintf(stderr, "ERROR: Failed to allocate stages (count=%d): %s\n", count, strerror(errno));
        return NULL;
    }
    stages->count = count;

    for (int i = 0; i < count; ++i)
    {
        if (parse_stage(s[i], stages->stages + i))
        {
            fprintf(stderr, "ERROR: Failed to parse stage %d\n", i + 1);
            goto free_stages;
        }
    }

    return stages;

free_stages:
    sdp_free_stages(stages);
    return NULL;
}

#ifdef WITH_UDEV
static hid_device *_open_device(sdp_udev *udev, uint16_t vid, uint16_t pid, const char *usb_path, bool quiet)
{
    hid_device *result = NULL;

    struct hid_device_info * const enumerator = hid_enumerate(vid, pid);
    if (!enumerator)
    {
        if (!quiet)
            fprintf(stderr, "ERROR: Failed to enumerate HID devices: %ls\n", hid_error(NULL));
        return NULL;
    }

    const char *device_path = NULL;
    for (struct hid_device_info *i = enumerator; !device_path && i; i = i->next)
    {
        if (!usb_path || sdp_udev_matching_usb_path(udev, i->path, usb_path))
            device_path = i->path;
    }

    if (device_path)
    {
        result = hid_open_path(device_path);
        if (!result && !quiet)
            fprintf(stderr, "ERROR: Failed to open device: %ls\n", hid_error(result));
    }
    else if (!quiet)
        fprintf(stderr, "ERROR: No matching device found\n");

    hid_free_enumeration(enumerator);

    return result;
}
#else
static hid_device *_open_device(sdp_udev *udev, uint16_t vid, uint16_t pid, const char *path, bool quiet)
{
    struct hid_device *result = hid_open(vid, pid, NULL);
    if (!result && !quiet)
        fprintf(stderr, "ERROR: Failed to open device: %ls\n", hid_error(result));
    return result;
}
#endif

static hid_device *open_device(uint16_t vid, uint16_t pid, const char *usb_path, bool wait)
{
    hid_device *result = NULL;

#ifdef WITH_UDEV
    sdp_udev *udev = sdp_udev_init();
    if (!udev)
    {
        fprintf(stderr, "ERROR: Failed to initialize udev\n");
        goto out;
    }

#else
    if (usb_path)
    {
        fprintf(stderr, "ERROR: Filtering by path is only supported with udev support\n");
        goto out;
    }
#endif

    result = _open_device(udev, vid, pid, usb_path, wait);
    if (!result)
    {
        if (!wait)
            goto free_udev;

        printf("Waiting for device...\n");

#ifdef WITH_UDEV
        const char *devpath = sdp_udev_wait(udev, vid, pid, usb_path, 20000);
        if (!devpath)
        {
            fprintf(stderr, "ERROR: Timeout!\n");
            goto free_udev;
        }
        result = hid_open_path(devpath);
        if (!result)
            fprintf(stderr, "ERROR: Failed to open device: %ls\n", hid_error(result));
#else
        do
        {
            usleep(500000ul); // 500ms
            result = hid_open(vid, pid, NULL);
        } while (!result);
#endif
    }

free_udev:
#ifdef WITH_UDEV
    sdp_udev_free(udev);
out:
#endif

    return result;
}

int sdp_execute_stages(sdp_stages *stages, bool initial_wait, const char *usb_path)
{
    int res = hid_init();
    if (res)
        fprintf(stderr, "ERROR: hidapi init failed\n");

    for (int i = 0; !res && i < stages->count; ++i)
    {
        struct stage *stage = stages->stages + i;
        printf("[Stage %d/%d] VID=0x%04x PID=0x%04x\n", i + 1, stages->count, stage->usb_vid, stage->usb_pid);

        bool wait = initial_wait || (i > 0);
        hid_device *handle = open_device(stage->usb_vid, stage->usb_pid, usb_path, wait);
        if (!handle)
        {
            res = 1;
            break;
        }

        uint32_t hab_status, status;
        res = sdp_error_status(handle, &hab_status, &status);
        if (res)
            break;

        if (sdp_execute_steps(handle, stage->steps))
        {
            fprintf(stderr, "ERROR: Failed to execute stage %d\n", i + 1);
            res = 1;
        }

        hid_close(handle);
    }

    if (hid_exit())
        fprintf(stderr, "ERROR: hidapi exit failed\n");

    if (!res)
        printf("All stages done\n");

    return res;
}

void sdp_free_stages(sdp_stages *stages)
{
    for (int i = 0; i < stages->count; ++i)
    {
        sdp_step *s = stages->stages[i].steps;
        while (s)
        {
            void *const to_be_freed = s;
            s = sdp_next_step(s);
            free(to_be_freed);
        }
    }
    free(stages);
}
