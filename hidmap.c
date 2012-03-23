
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
int add_mapper_signals(hidmap_device dev)
{
    printf("***************REPORT****************\n");
    char sig_name[256];
    int num_signals = 0;
    struct hid_element_info *elements, *cur_element;
    elements = hid_enumerate_elements(dev->handle);
    cur_element = elements;
    while (cur_element) {
        if (cur_element->type == 513) {
            cur_element = cur_element->next;
            continue;   // skip collections
        }

        sig_name[0] = 0;
        // Remove string "Generic Desktop" from path
        int len = strlen(cur_element->path), i, j = 0;
        for (i=0; i<len;) {
            if (strncmp(&cur_element->path[i], "Generic Desktop ", 16) == 0)
                i += 16;
            else if (isalnum(cur_element->path[i]) || cur_element->path[i] == '/') {
                sig_name[j++] = cur_element->path[i++];
                sig_name[j] = 0;
            }
            else
                i++;
        }
        mdev_add_output(dev->dev, sig_name, 1, 'i', 0, &cur_element->logical_range[0], &cur_element->logical_range[1]);
        num_signals++;

        printf("%s -> size: %i, bitoffset: %i\n", sig_name, cur_element->size, cur_element->bit_offset);
        cur_element = cur_element->next;
    }
    return num_signals;
}

// Check if any HID devices are available on the system
void scan_hid_devices()
{
    printf("Searching for HID devices...\n");
    char buffer[256], sig_name[256], serial_string[256];

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
        int len = strlen(buffer), i, j = 0;
        for (i=0; i<len;) {
            if (isalnum(buffer[i])) {
                sig_name[j++] = buffer[i++];
                sig_name[j] = 0;
            }
            else
                i++;
        }
        dev->dev = mdev_new(sig_name, port, 0);
        dev->vendor_id = cur_dev->vendor_id;
        dev->product_id = cur_dev->product_id;
        dev->serial_number = strdup(serial_string);
        dev->handle = hid_open(cur_dev->vendor_id, cur_dev->product_id, cur_dev->serial_number);
        if (add_mapper_signals(dev)) {
            hid_set_nonblocking(dev->handle, 1);
            printf("    Added %ls\n", cur_dev->product_string);
        }
        else {
            // device has no signals, free it but keep metadata for future scans
            mdev_free(dev->dev);
            dev->dev = 0;
            hid_close(dev->handle);
            dev->handle = 0;
        }

        dev->next = devices;
        devices = dev;
        
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
            if (temp->dev)
                mdev_poll(temp->dev, 0);
            if (temp->handle) {
                i = hid_read(temp->handle, data, 256);
                if (i) {
                    printf("read %i bytes from %s\n", i, mdev_name(temp->dev));
                }
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