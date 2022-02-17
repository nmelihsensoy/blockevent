/*
blockevent.c - blockevent for Android v0.3.0

Copyright 2022 N. Melih Sensoy

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h> 
#include <linux/input.h>
#include <signal.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>
#include <sys/poll.h>

#define VERSION_MAJOR "0"
#define VERSION_MINOR "3"
#define VERSION_PATCH "0"

#define test_bit(bit, array) (array[bit / 8] & (1 << (bit % 8)))

#define DEV_DIR "/dev/input"

static int nfds;
static int dev_ids[4];
static struct pollfd *pfds;

volatile sig_atomic_t e_flag = 0;
static void sig_handler(int signum);

enum {
    DEV_TOUCHSCREEN      = 1U << 0,
    DEV_VOLUP            = 1U << 1,
    DEV_VOLDOWN          = 1U << 2,
    DEV_ANY              = 1U << 3,
    ID_TOUCHSCREEN       = 0,
    ID_VOLDOWN           = 1,
    ID_VOLUP             = 2,
    ID_ANY               = 3
};

enum {
    ST_SCAN              = 1U << 0,
    ST_TRIGGER           = 1U << 1,
    PRINT_ERR            = 1U << 0,
    PRINT_NONE           = 1U << 1,
    PRINT_ALL            = 1U << 2,
    PRINT_ISSET          = 1U << 3,
};

static int open_device(const char *device, uint8_t classes, uint8_t print_flags)
{
    int fd;
    uint8_t abs_mask[ABS_MAX/8 + 1];
    uint8_t key_mask[KEY_MAX/8 + 1];
    uint8_t is_recognized = 0;

    fd = open(device, O_RDONLY | O_NONBLOCK);
    if(fd < 0) {
        if (print_flags & (PRINT_ERR | PRINT_ALL))
            fprintf(stderr, "Couldn't open '%s'.\n", device);
        return -1;
    }

    ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_mask)), abs_mask);
    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_mask)), key_mask);
    
    /*
    I followed here to get capabilities of devices.
    https://www.linuxjournal.com/article/6429

    I followed here to classify devices with using their capabilities.
    https://source.android.com/devices/input/touch-devices
    */
    if (classes & DEV_TOUCHSCREEN){
        if ((test_bit(ABS_MT_POSITION_X, abs_mask) && test_bit(ABS_MT_POSITION_Y, abs_mask)) 
            || (test_bit(ABS_X, abs_mask) && test_bit(ABS_Y, abs_mask))){
                if (test_bit(BTN_TOUCH, key_mask)){
                    if (print_flags & PRINT_ALL)
                        fprintf(stderr, "Touch device detected...\n");
                    dev_ids[ID_TOUCHSCREEN] = nfds;
                    is_recognized |= 1;
                }
        }  
    }
    if (classes & DEV_VOLDOWN && test_bit(KEY_VOLUMEDOWN, key_mask)){
        if (print_flags & PRINT_ALL)
            fprintf(stderr, "Volume down button device detected...\n");
        dev_ids[ID_VOLDOWN] = nfds;
        is_recognized |= 1;
    }
    if (classes & DEV_VOLUP && test_bit(KEY_VOLUMEUP, key_mask)){
        if (print_flags & PRINT_ALL)
            fprintf(stderr, "Volume up button device detected...\n");
        dev_ids[ID_VOLUP] = nfds;
        is_recognized |= 1;
    }
    if (classes & DEV_ANY){
        if (print_flags & PRINT_ALL)
            fprintf(stderr, "Device detected...\n");
        dev_ids[ID_ANY] = nfds;
        is_recognized |= 1;
    }

    if (is_recognized & 1){
        if (print_flags & PRINT_ALL)
            fprintf(stderr, "Stored device count: %d. Storing detected device...\n", nfds);
        pfds = realloc(pfds, sizeof(struct pollfd) * (nfds + 1));
        pfds[nfds].fd = fd;
        pfds[nfds].events = POLLIN;
        nfds++;
        return 0;
    }

    close(fd);
    return 1;
}

static void close_devices(uint8_t print_flags)
{
    int i;
    if (print_flags & PRINT_ALL)
                fprintf(stderr, "Total device: %d\n", nfds);
    for(i = 0; i < nfds; i++) {
        if(pfds[i].fd >= 0){
            if (print_flags & PRINT_ALL)
                fprintf(stderr, "%d.device closing...\n", i);
            close(pfds[i].fd);
            free(&pfds[i]);
        }
    }
    free(pfds);
}

static int scan_devices(uint8_t classes, uint8_t print_flags)
{
    char devloc[24]; //Assumed size for 'dev/input/eventXXX'
    char *devname;
    DIR *dir;
    struct dirent *dp;
    if ((dir = opendir(DEV_DIR)) == NULL) return -1;
    devname = malloc(sizeof(char) * 8); //Assumed size for 'eventXXX'
    strcpy(devname, "event");
    uint8_t temp = classes;
    while ((dp = readdir(dir)) != NULL) {
        //Avoiding ., .., mouseX, tsX, jsX
        if (dp->d_name[0] != 'e') continue;
        if (dp->d_name[4] != 't' || dp->d_name[8] != '\0') continue;
        
        devname[5] = dp->d_name[5];
        devname[6] = dp->d_name[6];
        devname[7] = dp->d_name[7];

        snprintf(devloc, 24, "%s/%s", DEV_DIR, devname);
        if (print_flags & PRINT_ALL)
            fprintf(stderr, "Found device: '%s'. Opening...\n", devloc);
        if (open_device(devloc, classes, print_flags) == 0) 
            temp &= temp - 1; //See brian kernighan's bit counting algorithm.
        if (temp == 0) break;
    }
    closedir(dir);
    free(devname);
    return 0;
}

static void usage(char *name)
{
    fprintf(stderr, "Usage: %s [-t] [-d input device] [-s trigger] [-v level]\n", name);
    fprintf(stderr, "    -t: block touch screen.\n");
    fprintf(stderr, "    -d: block input device.Format:'/dev/input/eventX'. Use 'getevent' to find eventX.\n");
    fprintf(stderr, "    -s: stop trigger (Volume Down='v_dwn', Volume Up='v_up')\n");
    fprintf(stderr, "    -v: verbosity level (Errors=1, None=2, All=4)\n");
    fprintf(stderr, "    -h: print help.\n");
    fprintf(stderr, "\nblockevent for Android v%s.%s.%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    fprintf(stderr, "https://github.com/nmelihsensoy/blockevent\n");
}

int main(int argc, char *argv[])
{   
    int c;
    int res;
    uint8_t flags = 0;
    uint8_t classes = 0;
    uint8_t trigger_device;
    uint8_t blocking_device;
    uint8_t print_flags = 0;
    struct input_event event;
    const char *device = NULL;
    const char *trigger = NULL;

    opterr = 0;
    do{
        c = getopt (argc, argv, "htd:s:v:");
        if (c == EOF)
            break;
        switch (c){
        case 't':
            classes |= DEV_TOUCHSCREEN;
            flags |= ST_SCAN;
            blocking_device = ID_TOUCHSCREEN;
            break;
        case 'd':
            device = optarg;
            blocking_device = ID_ANY;
            break;
        case 's':
            flags |= ST_TRIGGER;
            trigger = optarg;
            break;
        case 'v':
            print_flags |= strtoul(optarg, NULL, 0);
            print_flags |= PRINT_ISSET;
            break;
        case '?':
            if (optopt == 'd' || optopt == 's' || optopt == 'v')
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint(optopt))
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
            fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
            return 1;
        case 'h':
            usage(argv[0]);
            exit(1);
        }
    }while (1);

    if (argc < 2) {
        usage(argv[0]);
        exit(1);
    }

    if ((print_flags & PRINT_ISSET) == 0)
        print_flags |= PRINT_ERR;

    nfds = 0;
    signal(SIGINT, sig_handler);

    if (flags & ST_TRIGGER){
        if (print_flags & PRINT_ALL)
            fprintf(stderr, "Trigger: %s\n", trigger);
        if (strcmp(trigger, "v_dwn") == 0){
            classes |= DEV_VOLDOWN;
            flags |= ST_SCAN;
            trigger_device = ID_VOLDOWN;
        }else if (strcmp(trigger, "v_up") == 0){
            classes |= DEV_VOLUP;
            flags |= ST_SCAN;
            trigger_device = ID_VOLUP;
        }else{
            if (print_flags & (PRINT_ERR | PRINT_ALL))
                fprintf(stderr, "Invalid trigger.Aborted.\n");
            return -1;
        }
    }

    if (flags & ST_SCAN){
        if (print_flags & PRINT_ALL)
            fprintf(stderr, "Scanning for devices...\n");
        res = scan_devices(classes, print_flags);
        if (res < 0){
            if (print_flags & (PRINT_ERR | PRINT_ALL))
                fprintf(stderr, "Device scan failed in %s.\n", DEV_DIR);
            return -1;
        }
    }

    if (device){
        res = open_device(device, DEV_ANY, print_flags);
        if (res < 0)
            return -1;
    }

    blocking_device = dev_ids[blocking_device];

    if (print_flags & PRINT_ALL){
        fprintf(stderr, "Stored device count: %d\n", nfds);
        fprintf(stderr, "Blocking...\n");
    }
            
    if (ioctl(pfds[blocking_device].fd, EVIOCGRAB, 1) < 0){
        if (print_flags & (PRINT_ERR | PRINT_ALL))
                fprintf(stderr, "Couldn't get exclusive access.Blocking aborted.\n");
        close_devices(print_flags);
        return -1;
    }
    
    if ((flags & ST_TRIGGER) == 0){
        if (print_flags & PRINT_ALL)
            fprintf(stderr, "Pausing...\n");
        pause();
    }

    if (flags & ST_TRIGGER){
        if (print_flags & PRINT_ALL)
            fprintf(stderr, "Listening for a stop trigger...\n");
        trigger_device = dev_ids[trigger_device];
        while(1){
            if (e_flag) break;
            poll(&pfds[trigger_device], 1, -1); //Polling the trigger device only.
            if(pfds[trigger_device].revents){
                if(pfds[trigger_device].revents & POLLIN){
                    res = read(pfds[trigger_device].fd, &event, sizeof(event));
                    if(res < (int)sizeof(event)){
                        if (print_flags & (PRINT_ERR | PRINT_ALL))
                            fprintf(stderr, "Couldn't get event for trigger device.\n");
                        return 1;
                    }
                    if (print_flags & PRINT_ALL)
                        fprintf(stderr, "Dev: [%d] Event: Type[%d] Code[%d] Value[%d]\n", 
                            trigger_device, event.type, event.code, event.value);
                    
                    if (event.code == KEY_VOLUMEUP || event.code == KEY_VOLUMEDOWN){
                        e_flag = 1;
                        if (print_flags & PRINT_ALL)
                            fprintf(stderr, "Volume key report detected.Stop listening...\n");
                    }
                }
            }
        }
    }

    if (print_flags & PRINT_ALL)
        fprintf(stderr, "Releasing...\n");
    ioctl(pfds[blocking_device].fd, EVIOCGRAB, 0);
    close_devices(print_flags);
    if (print_flags & PRINT_ALL)
        fprintf(stderr, "Exiting...\n");
    return 0;
}

static void sig_handler(int signum)
{
    e_flag = 1;
}