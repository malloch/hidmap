#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>
#include <wchar.h>
#include <locale.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#include "hidapi.h"
#include "mapper/mapper.h"

typedef struct _hidmap_device {
    mapper_device   mapper_dev;
    IOHIDDeviceRef  device_ref;
    unsigned short  vendor_id;
    unsigned short  product_id;
    char            *serial_number;
    int             is_linked;
    struct _hidmap_device *next;
} *hidmap_device;

void scan_hid_devices();
void cleanup_all_devices();
int hid_device_load_elements(hidmap_device hdev)
;