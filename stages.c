#include "stages.h"
#include "steps.h"
#include "sdp.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ENABLE_UDEV
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
        fprintf(stderr, "ERROR: Device steps \"%s\" invalid\n", s);
        return 1;
    }

    unsigned int vid, pid;
    int conversions = sscanf(tok, "%04x:%04x", &vid, &pid);
    if (conversions != 2)
    {
        fprintf(stderr, "ERROR: Device steps didn't contain USB VID/PID\n");
        if (errno != 0)
            fprintf(stderr, ": %s\n", strerror(errno));
        else
            fputc('\n', stderr);
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

static hid_device *open_device(uint16_t vid, uint16_t pid, bool wait)
{
    hid_device *result = NULL;

#ifdef ENABLE_UDEV
    sdp_udev *udev = sdp_udev_init();
    if (!udev)
    {
        fprintf(stderr, "ERROR: Failed to initialize udev\n");
        goto out;
    }
#endif

    result = hid_open(vid, pid, NULL);
    if (!result)
    {
        if (!wait)
            goto free_udev;

        printf("Waiting for device...\n");

#ifdef ENABLE_UDEV
        const char *devpath = sdp_udev_wait(udev, vid, pid, 5000);
        if (!devpath)
        {
            fprintf(stderr, "ERROR: Timeout!\n");
            goto free_udev;
        }
        result = hid_open_path(devpath);
#else
        do {
            usleep(500000ul); // 500ms
            result = hid_open(vid, pid, NULL);
        } while (!result);
#endif
    }

free_udev:
#ifdef ENABLE_UDEV
    sdp_udev_free(udev);
out:
#endif

    return result;
}

int sdp_execute_stages(sdp_stages *stages)
{
    int res = hid_init();
	if (res)
		fprintf(stderr, "ERROR: hidapi init failed\n");

    for (int i=0; !res && i<stages->count; ++i)
    {
        struct stage *stage = stages->stages + i;
        printf("[Stage %d/%d] VID=0x%04x PID=0x%04x\n", i+1, stages->count, stage->usb_vid, stage->usb_pid);

        hid_device *handle = open_device(stage->usb_vid, stage->usb_pid, true);
        if (!handle)
        {
            fprintf(stderr, "ERROR: Failed to open device: %ls\n", hid_error(handle));
            res = 1;
            break;
        }

        uint32_t hab_status, status;
        res = sdp_error_status(handle, &hab_status, &status);
        if (res)
            fprintf(stderr, "ERROR: Failed to read error status\n");
        else
            printf("HAB status: 0x%08x; status: 0x%08x\n", hab_status, status);

        if (sdp_execute_steps(handle, stage->steps))
        {
            fprintf(stderr, "ERROR: Failed to execute step\n");
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
