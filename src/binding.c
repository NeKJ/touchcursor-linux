#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include "binding.h"

// The input device
int input;

// The output device
int output;

/**
 * Binds to the input device using ioctl.
 */
void bindInput(char* eventPath)
{
    // Open the keyboard device
    fprintf(stdout, "info: attempting to attach to: '%s'\n", eventPath);
    input = open(eventPath, O_RDONLY);
    if (input < 0)
    {
        fprintf(stdout, "error: cannot open the input device, is this file set to the 'input' group or equivalent?: %s.\n", strerror(errno));
        return;
    }

    // Retrieve the device name
    char keyboardName[256] = "Unknown";
    if (ioctl(input, EVIOCGNAME(sizeof(keyboardName) - 1), keyboardName) < 0)
    {
        fprintf(stdout, "error: cannot get the device name: %s\n", strerror(errno));
        return;
    }
    else
    {
        fprintf(stdout, "info: attached to: %s\n", keyboardName);
    }
    // Check that the device is not our virtual device
    if (strcasestr(keyboardName, "Virtual TouchCursor Keyboard") != NULL)
    {
        fprintf(stdout, "error: cannot attach to the virtual device: %s.\n", strerror(errno));
        return;
    }
    // Allow last key press to go through
    // Grabbing the keys too quickly prevents the last key up event from being sent
    // https://bugs.freedesktop.org/show_bug.cgi?id=101796
    usleep(200 * 1000);
    // Grab keys from the input device
    if (ioctl(input, EVIOCGRAB, 1) < 0)
    {
        fprintf(stdout, "error: EVIOCGRAB: %s.\n", strerror(errno));
        return;
    }
    fprintf(stdout, "info: successfully captured input device\n");
}

/**
 * Creates and binds a virtual output device using ioctl and uinput.
 */
void bindOutput()
{
    // Define the virtual keyboard
    struct uinput_user_dev virtualKeyboard;
    memset(&virtualKeyboard, 0, sizeof(virtualKeyboard));
    snprintf(virtualKeyboard.name, UINPUT_MAX_NAME_SIZE, "Virtual TouchCursor Keyboard");
    virtualKeyboard.id.bustype = BUS_USB;
    virtualKeyboard.id.vendor  = 0x01;
    virtualKeyboard.id.product = 0x01;
    virtualKeyboard.id.version = 1;

    // Open the output
    output = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (output < 0)
    {
        fprintf(stdout, "error: failed to open /dev/uinput: %s.\n", strerror(errno));
        return;
    }
    // Enable key press/release event
    if (ioctl(output, UI_SET_EVBIT, EV_KEY) < 0)
    {
        fprintf(stdout, "error: cannot set EV_KEY on output: %s.\n", strerror(errno));
    }
    // Enable set of KEY events
    for (int i = 0; i < KEY_MAX; i++)
    {
        if (ioctl(output, UI_SET_KEYBIT, i) < 0)
        {
            fprintf(stdout, "error: cannot set key bit: %s.\n", strerror(errno));
            return;
        }
    }
    // Enable synchronization event
    if (ioctl(output, UI_SET_EVBIT, EV_SYN) < 0)
    {
        fprintf(stdout, "error: cannot set EV_SYN on output: %s\n", strerror(errno));
    }
    // Write the uinput_user_dev structure into uinput file descriptor
    if (write(output, &virtualKeyboard, sizeof(virtualKeyboard)) < 0)
    {
        fprintf(stdout, "error: cannot write uinput_user_dev struct into uinput file descriptor: %s\n", strerror(errno));
    }
    // create the device via an IOCTL call 
    if (ioctl(output, UI_DEV_CREATE) < 0)
    {
        fprintf(stdout, "error: ioctl: UI_DEV_CREATE: %s\n", strerror(errno));
    }
    fprintf(stdout, "info: successfully created virtual output device\n");
}