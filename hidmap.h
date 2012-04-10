
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>
#include <wchar.h>
#include <locale.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#include "HID_Utilities_External.h"
#include "hidapi.h"
#include "mapper/mapper.h"

void declare_mapper_device();
void declare_mapper_signals(mapper_device dev);
void scan_hid_devices();