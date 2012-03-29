
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

#include "hidmap_osx.h"

#define MAXLIST 256

int done = 0;
int port = 9000;
unsigned char data[MAXLIST];

void cleanup_device(hidmap_device dev);



void loop()
{
    while (!done) {
        scan_hid_devices();
        usleep(10 * 1000 * 1000);
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