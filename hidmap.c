
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>
#include <wchar.h>
#include <locale.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hidmap.h"

#include <unistd.h>
#include <signal.h>

#define MAXLIST 256

int done = 0;
int port = 9000;
unsigned char data[MAXLIST];

typedef struct _hidmap_device {
    mapper_device   dev;
    hid_device      *handle;
    unsigned short  vendor_id;
    unsigned short  product_id;
    char            *serial_number;
    int             is_linked;
    struct _hidmap_device *next;
} *hidmap_device;

struct _hidmap_device *devices = 0;

void cleanup_device(hidmap_device dev);

// Declare output signals
void add_mapper_signals(hidmap_device dev)
{
    printf("***************REPORT****************\n");
    data[0] = 0x2;
    int len = hid_get_feature_report(dev->handle, data, MAXLIST);
    for (int i = 0; i < len; i++) {
        printf("%c", data[i]);
    }
    printf("\n");
}

// Check if any HID devices are available on the system
void scan_hid_devices()
{
    printf("Searching for HID devices...\n");
    char buffer[256], serial_string[256], *position;

    struct hid_device_info *devs, *cur_dev;
    devs = hid_enumerate(0x0, 0x0);
    cur_dev = devs;
    while (cur_dev) {
        buffer[0] = 0;
        if (cur_dev->serial_number)
            snprintf(serial_string, 256, "%ls", cur_dev->serial_number);
        else
            serial_string[0] = 0;
        hidmap_device temp = devices;
        while (temp) {
            if ((temp->vendor_id == cur_dev->vendor_id) &&
                (temp->product_id == cur_dev->product_id) &&
                (strcmp(temp->serial_number, serial_string) == 0))
                break;
            temp = temp->next;
        }
        if (temp) {
            cur_dev = cur_dev->next;
            continue;
        }
        // new device discovered
        hidmap_device dev = (hidmap_device) calloc(1, sizeof(struct _hidmap_device));
        snprintf(buffer, 256, "%ls", cur_dev->product_string);
        while (position = strchr(buffer, ' ')) {
            *position = '_';
        }
        dev->dev = mdev_new(buffer, port, 0);
        dev->vendor_id = cur_dev->vendor_id;
        dev->product_id = cur_dev->product_id;
        dev->serial_number = strdup(serial_string);
        dev->handle = hid_open(cur_dev->vendor_id, cur_dev->product_id, cur_dev->serial_number);
        hid_set_nonblocking(dev->handle, 1);
        dev->next = devices;
        devices = dev;
        add_mapper_signals(dev);
        printf("    Added %ls\n", cur_dev->product_string);
        cur_dev = cur_dev->next;
    }
    hid_free_enumeration(devs);
}

void cleanup_device(hidmap_device dev)
{
    if (dev->dev) {
        mdev_free(dev->dev);
    }
    if (dev->handle) {
        hid_close(dev->handle);
    }
    if (dev->serial_number) {
        free(dev->serial_number);
    }
}

void cleanup_all_devices()
{
    printf("\nCleaning up!\n");
    hidmap_device dev;
    while (devices) {
        dev = devices;
        devices = dev->next;
        cleanup_device(dev);
    }
}

void loop()
{
    int counter = 0;
    unsigned char data[256];
    hidmap_device temp;
    scan_hid_devices();
    int i;
    while (!done) {
        // poll libmapper outputs
        temp = devices;
        while (temp) {
            mdev_poll(temp->dev, 0);
            i = hid_read(temp->handle, data, 256);
            if (i) {
                printf("read %i bytes from %s\n", i, mdev_name(temp->dev));
            }
            temp = temp->next;
        }
        usleep(10 * 1000);
        if (counter++ > 500) {
            scan_hid_devices();
            counter = 0;
        }
    }
}

void ctrlc(int sig)
{
    done = 1;
}

int main ()
{
    signal(SIGINT, ctrlc);

    loop();
    
done:
    cleanup_all_devices();
    hid_exit();
    return 0;
}