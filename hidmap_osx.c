/*
 *  hidmap_osx.c
 *  hidmap
 *
 *  Created by jocal on 12-03-28.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */

#include "hidapi.h"
#include "hidmap.h"
#include "hidmap_osx.h"
#include "HID_Utilities.h"


static IOHIDManagerRef hid_manager = 0x0;

hidmap_device hidmap_device_list = 0;

void read_hid_device(hidmap_device dev)
{
    //CFIndex logical = 0;
    //if ( kIOReturnSuccess == IOHIDDeviceGetValue( gCurrentIOHIDDeviceRef, gCurrenelement_ref, &tIOHIDValueRef ) )
    //    logical = IOHIDValueGetIntegerValue( tIOHIDValueRef );
    
}

static void process_pending_events() {
	SInt32 res;
	do {
		res = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, FALSE);
	} while(res != kCFRunLoopRunFinished && res != kCFRunLoopRunTimedOut);
}

void cleanup_device(hidmap_device dev)
{
    if (dev->mapper_dev) {
        mdev_free(dev->mapper_dev);
    }
    free(dev);
}

void cleanup_all_devices()
{
    printf("\nCleaning up!\n");
    hidmap_device dev;
    while (hidmap_device_list) {
        dev = hidmap_device_list;
        hidmap_device_list = dev->next;
        cleanup_device(dev);
    }
}

void device_value_handler(void *context, IOReturn result, void *sender, IOHIDValueRef value)
{
    hidmap_device hdev = (hidmap_device)context;
    if (!hdev)
        return;

    printf("%s: value handler", mdev_name(hdev->mapper_dev));
}

void device_report_handler(void *context, IOReturn result, void *sender, IOHIDReportType type, 
                           uint32_t reportID, uint8_t *report, CFIndex reportLength)
{
    printf("report handler!\n");
}

void device_removal_handler(void *context, IOReturn result, void *sender)
{
    printf("device_removal handler!\n");
    hidmap_device hdev = (hidmap_device)context;
    if (!hdev)
        return;
    if (!hdev->mapper_dev)
        return;
    printf("%s: removal handler", mdev_name(hdev->mapper_dev));
}

void scan_hid_devices()
{
    CFIndex num_devices;
    
    if (!hid_manager) {
        hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
        if (!hid_manager)
            return;
        IOHIDManagerSetDeviceMatching(hid_manager, NULL);
        IOHIDManagerScheduleWithRunLoop(hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    }
    
    process_pending_events();
    
    CFSetRef device_set = IOHIDManagerCopyDevices(hid_manager);
    if (!device_set)
        return;
    num_devices = CFSetGetCount(device_set);
    IOHIDDeviceRef *device_array = calloc(num_devices, sizeof(IOHIDDeviceRef));
    CFSetGetValues(device_set, (const void **)device_array);

    // Iterate over each device
    CFIndex i, count = CFSetGetCount(device_set);
    for (i = 0; i < count; i++) {
        char cbuf[256];
        IOHIDDeviceRef device = device_array[i];
        if (!device)
            continue;

        // check if device already recorded
        hidmap_device temp = hidmap_device_list;
        while (temp) {
            if (temp->device_ref == device) {
                break;
            }
            temp = temp->next;
        }
        if (temp)
            continue;
        // get device name
        CFStringRef str = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
        CFStringGetCString(str, cbuf, 256, kCFStringEncodingUTF8);

        // tweak the name a bit
        char dev_name[256];
        int len = strlen(cbuf), i, j = 0;
        for (i=0; i<len;) {
            if (strncmp(cbuf+i, "Generic Desktop ", 16) == 0)
                i += 16;
            else if (strncmp(cbuf+i, " or Keypad ", 11) == 0) {
                dev_name[j++] = '/';
                i += 11;
            }
            else if (strncmp(cbuf+i, " #0x", 3) == 0) {
                dev_name[j++] = '/';
                i += 2;
            }
            else if (isalnum(cbuf[i]) || cbuf[i] == '/') {
                dev_name[j++] = cbuf[i++];
                dev_name[j] = 0;
            }
            else
                i++;
        }

        // add a device record
        hidmap_device hdev = (hidmap_device) calloc(1, sizeof(struct _hidmap_device));
        hdev->device_ref = device;

        hdev->mapper_dev = mdev_new(dev_name, 9000, 0);
        hdev->next = hidmap_device_list;
        hidmap_device_list = hdev;

        if (!hid_device_load_elements(hdev)) {
            // this device has no signals - free it
            mdev_free(hdev->mapper_dev);
        }
        uint8_t *report;
        CFIndex report_length;

        // register for input callback
        IOHIDDeviceRegisterInputReportCallback(device, report, report_length, device_report_handler, hdev);
        IOHIDDeviceRegisterInputValueCallback(device, device_value_handler, hdev);

        // register for device removal callback
        IOHIDDeviceRegisterRemovalCallback(device, device_removal_handler, hdev);
    }
}

int hid_device_load_elements(hidmap_device hdev)
{
    char cbuf[256];
    printf("hid_device_load_elements\n");
    int num_elements = 0;

    if (!hdev) return 0;
    if (!hdev->device_ref) return 0;

    CFArrayRef element_array = IOHIDDeviceCopyMatchingElements(hdev->device_ref, NULL, 0);
    if (!element_array)
        return 0;

	/* Iterate over each element. */	
    CFIndex i, count = CFArrayGetCount(element_array);
	for (i = 0; i < count; i++) {
        IOHIDElementRef element = (IOHIDElementRef) CFArrayGetValueAtIndex(element_array, i);
        if (!element)
            continue;
        
        IOHIDElementType element_type = IOHIDElementGetType(element);
        if (element_type > kIOHIDElementTypeInput_ScanCodes)
            continue;   // skip non-input element types for now
        
        /* Get full path */
        CFMutableStringRef full_path = NULL;
        IOHIDElementRef current_element = element;

        while (current_element) {
            CFStringRef name = IOHIDElementGetName(current_element);
            CFStringRef usage = NULL;
            if (!name) {
                uint32_t usage_page_id = IOHIDElementGetUsagePage(current_element);
                uint32_t usage_id = IOHIDElementGetUsage(current_element);
                name = usage = HIDCopyUsageName(usage_page_id, usage_id);
            }
            if (name) {
                if (full_path) {
                    CFStringInsert(full_path, 0, CFSTR("/"));
                    CFStringInsert(full_path, 0, name);
                }
                else {
                    full_path = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, name);
                }
                cbuf[0] = 0;
                CFStringGetCString(full_path, cbuf, 256, kCFStringEncodingUTF8);
            }
            else {
                full_path = NULL;
                break;
            }
            if (usage) {
                CFRelease(usage);
            }
            current_element = IOHIDElementGetParent(current_element);
        }
        if (!full_path)
            continue;

        num_elements++;

        // Tweak the path a bit
        CFStringGetCString(full_path, cbuf, 256, kCFStringEncodingUTF8);

        char sig_name[256];
        int len = strlen(cbuf), i, j = 0;
        for (i=0; i<len;) {
            if (strncmp(cbuf+i, "Generic Desktop ", 16) == 0)
                i += 16;
            else if (strncmp(cbuf+i, " or Keypad ", 11) == 0) {
                sig_name[j++] = '/';
                i += 11;
            }
            else if (strncmp(cbuf+i, " #0x", 3) == 0) {
                sig_name[j++] = '/';
                i += 2;
            }
            else if (isalnum(cbuf[i]) || cbuf[i] == '/') {
                sig_name[j++] = cbuf[i++];
                sig_name[j] = 0;
            }
            else
                i++;
        }

        /* Register the signal */
        printf("--> %s\n", sig_name);
        //mdev_add_output(dev->mapper_dev, cbuf, 1, i, 0,
        //                IOHIDElementGetLogicalMin(element),
        //                IOHIDElementGetLogicalMax(element));
	}
    return num_elements;
}
