/*
blockevent.c - blockevent for Android v0.4.3

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
#include <stdbool.h>

#define VERSION_MAJOR "0"
#define VERSION_MINOR "4"
#define VERSION_PATCH "3"

#define DEV_DIR "/dev/input"

static int nfds;
static int dev_ids[2];
static struct pollfd *pfds;

volatile sig_atomic_t e_flag = 0;
static void sig_handler(int signum);

enum {
    DEV_TOUCHSCREEN      = 1U << 0,
    DEV_VOLUP            = 1U << 1,
    DEV_VOLDOWN          = 1U << 2,
    DEV_ANY              = 1U << 3,
    DEV_POWERBTN         = 1U << 4,
    DEV_BLOCKING         = 1U << 5,
    DEV_TRIGGER          = 1U << 6,
    ID_BLOCKING          = 0,  
    ID_TRIGGER           = 1,
};

enum {
    ST_SCAN              = 1U << 0,
    ST_TRIGGER           = 1U << 1,
    ST_CUSTOM_TRIGGER    = 1U << 2,
    PRINT_ERR            = 1U << 0,
    PRINT_NONE           = 1U << 1,
    PRINT_ALL            = 1U << 2,
    PRINT_ISSET          = 1U << 3,
};

static inline bool test__bit(unsigned bit, unsigned char *array)
{
  return array[bit / 8] & (1 << (bit % 8));
}

static inline void print_err(const char *format, const char *text, 
    int number, uint8_t flags)
{
    if (flags & (PRINT_ERR | PRINT_ALL))
        text != NULL ? fprintf(stderr, format, text) : fprintf(stderr, format, number);
}

static inline void print_all(const char *format, const char *text, 
    int number, uint8_t flags)
{
    if (flags & PRINT_ALL)
        text != NULL ? fprintf(stderr, format, text) : fprintf(stderr, format, number);     
}

static inline void print_event(struct input_event *ev, int dev_id)
{
    fprintf(stderr, "Dev: [%d] Event: Type[%d] Code[%d] Value[%d]\n", 
        dev_id, ev->type, ev->code, ev->value);
}

static int open_device(const char *device, uint8_t classes, uint8_t print_flags)
{
    int fd;
    uint8_t abs_mask[ABS_CNT / 8];
    uint8_t key_mask[KEY_CNT / 8 ];
    uint8_t prop_mask[INPUT_PROP_CNT / 8];
    uint8_t is_recognized = 0;

    print_all("'%s': device opening...\n", device, 0, print_flags);
    fd = open(device, O_RDONLY | O_NONBLOCK);
    if(fd < 0) {
        print_err("Couldn't open '%s'.\n", device, 0, print_flags);
        return -1;
    }

    ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_mask)), abs_mask);
    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_mask)), key_mask);
    ioctl(fd, EVIOCGPROP(sizeof(prop_mask)), prop_mask);
    
    if ((is_recognized == 0) && classes & DEV_ANY){
        print_all("'DEV_ANY' detected.\n", NULL, 0, print_flags);
        is_recognized |= 1;
    }
    /*
    I followed here to get capabilities of devices.
    https://www.linuxjournal.com/article/6429

    I followed here to classify devices with using their capabilities.
    https://source.android.com/devices/input/touch-devices
    */
    if ((is_recognized == 0) && (classes & DEV_TOUCHSCREEN)){
        if ((test__bit(ABS_MT_POSITION_X, abs_mask) && test__bit(ABS_MT_POSITION_Y, abs_mask)) 
            || (test__bit(ABS_X, abs_mask) && test__bit(ABS_Y, abs_mask))){
                if (test__bit(BTN_TOUCH, key_mask) && test__bit(INPUT_PROP_DIRECT, prop_mask)){
                    print_all("'DEV_TOUCHSCREEN' detected...\n\n", NULL, 0, print_flags);
                    is_recognized |= 1;
                }
        }  
    }

    if ((is_recognized == 0) && !test__bit(KEY_MEDIA, key_mask)){    
        if ((is_recognized == 0) && (classes & DEV_VOLDOWN) && test__bit(KEY_VOLUMEDOWN, key_mask)){
            print_all("'DEV_VOLDOWN' detected...\n\n", NULL, 0, print_flags);
            is_recognized |= 1;
        }
        if ((is_recognized == 0) && (classes & DEV_VOLUP) && test__bit(KEY_VOLUMEUP, key_mask)){
            print_all("'DEV_VOLUP' detected...\n\n", NULL, 0, print_flags);
            is_recognized |= 1;
        }
        if ((is_recognized == 0) && (classes & DEV_POWERBTN) && test__bit(KEY_POWER, key_mask)){
            print_all("'DEV_POWERBTN' detected...\n\n", NULL, 0, print_flags);
            is_recognized |= 1;
        }
    }

    if (is_recognized & 1){
        print_all("Storing detected device...\n", NULL, 0, print_flags);
        pfds = realloc(pfds, sizeof(struct pollfd) * (nfds + 1));
        pfds[nfds].fd = fd;
        pfds[nfds].events = POLLIN;
        if (classes & DEV_BLOCKING)
            dev_ids[ID_BLOCKING] = nfds;

        if (classes & DEV_TRIGGER)
            dev_ids[ID_TRIGGER] = nfds;

        nfds++;
        return 0;
    }

    print_all("Closing device...\n", NULL, 0, print_flags);
    close(fd);
    return 1;
}

static void close_devices(uint8_t print_flags)
{
    int i;
    print_all("Opened devices: %d\n", NULL, nfds, print_flags);
    for(i = 0; i < nfds; i++) {
        if(pfds[i].fd >= 0){
            print_all("%d.device closing...\n", NULL, i, print_flags);
            close(pfds[i].fd);
        }
    }
    if (pfds != NULL){
        free(pfds);
    }
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

        snprintf(devloc, sizeof(devloc), "%s/%s", DEV_DIR, devname);

        print_all("'%s' found. Opening...\n", devloc, 0, print_flags);
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
    fprintf(stderr, "    -s: stop trigger.(Power Button='pwr', Volume Down='v_dwn', Volume Up='v_up')\n");
    fprintf(stderr, "                     (Custom Trigger='/dev/input/eventX:KEYCODE'.See 'input-event-codes.h')\n");
    fprintf(stderr, "    -v: verbosity level (Errors=1, None=2, All=4)\n");
    fprintf(stderr, "    -h: print help.\n");
    fprintf(stderr, "\nblockevent for Android v%s.%s.%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    fprintf(stderr, "https://github.com/nmelihsensoy/blockevent\n");
}

int main(int argc, char *argv[])
{   
    int c;
    int res, res_tgr;
    uint8_t flags = 0;
    uint8_t classes = 0;
    uint8_t trigger_device;
    uint8_t blocking_device;
    uint8_t print_flags = 0;
    uint16_t trigger_event;
    struct input_event event;
    const char *device = NULL;
    char *trigger_dev = NULL;
    char *event_tmp = NULL;

    opterr = 0;
    do{
        c = getopt (argc, argv, "htd:s:v:");
        if (c == EOF)
            break;
        switch (c){
        case 't':
            classes |= DEV_TOUCHSCREEN | DEV_BLOCKING;
            flags |= ST_SCAN;
            break;
        case 'd':
            device = optarg;
            break;
        case 's':
            flags |= ST_TRIGGER | DEV_TRIGGER;
            trigger_dev = optarg;
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
        return 1;
    }

    if ((print_flags & PRINT_ISSET) == 0)
        print_flags |= PRINT_ERR;

    nfds = 0;
    signal(SIGINT, sig_handler);

    if (flags & ST_TRIGGER){
        print_all("'%s' : stop trigger.\n", trigger_dev, 0, print_flags);
        if (strcmp(trigger_dev, "v_dwn") == 0){
            classes |= DEV_VOLDOWN;
            flags |= ST_SCAN;
            trigger_event = KEY_VOLUMEDOWN;
        }else if (strcmp(trigger_dev, "v_up") == 0){
            classes |= DEV_VOLUP;
            flags |= ST_SCAN;
            trigger_event = KEY_VOLUMEUP;
        }else if (strcmp(trigger_dev, "pwr") == 0){
            classes |= DEV_POWERBTN;
            flags |= ST_SCAN;
            trigger_event = KEY_POWER;
        }else{
            trigger_dev = strtok(trigger_dev, ":");
            event_tmp = strtok(NULL, ":");
            if (event_tmp == NULL){
                print_err("Event code can't be empty.\n", NULL, 0, print_flags);
                return -1;
            } 
            trigger_event = atoi(event_tmp);  
            print_all("'%s': dev", trigger_dev, 0, print_flags);
            print_all(" '%d': event code\n", NULL, trigger_event, print_flags);
            flags |= ST_CUSTOM_TRIGGER; 
        }
    }

    if (flags & ST_CUSTOM_TRIGGER){
        res_tgr = open_device(trigger_dev, DEV_ANY|DEV_TRIGGER, print_flags);
            if (res_tgr < 0){
                print_err("'%s': failed to open.\n", trigger_dev, 0, print_flags);
                return -1;
            }
    }

    if (flags & ST_SCAN){
        print_all("Scanning for devices...\n", NULL, 0, print_flags);
        res = scan_devices(classes, print_flags);
        if (res < 0){
            print_err("Device scan failed in %s.\n", DEV_DIR, 0, print_flags);
            return -1;
        }
    }

    if (device){
        res = open_device(device, DEV_ANY|DEV_BLOCKING, print_flags);
        if (res < 0)
            return -1;
    }

    blocking_device = dev_ids[ID_BLOCKING];
    
    print_all("Opened device count: %d\nBlocking device...\n", NULL, nfds, print_flags);
    
    if (&pfds[blocking_device] == NULL){
        print_err("No device found to block. Exit.\n", NULL, 0, print_flags);
        return 1;
    }

    if (ioctl(pfds[blocking_device].fd, EVIOCGRAB, 1) < 0){
        print_err("Couldn't get exclusive access.Blocking aborted.\n", NULL, 0, print_flags);
        close_devices(print_flags);
        return -1;
    }
    
    if ((flags & ST_TRIGGER) == 0){
        print_all("Pausing...\n", NULL, 0, print_flags);
        pause();
    }

    if (flags & ST_TRIGGER){
        print_all("Stop trigger : listening...\n", NULL, 0, print_flags);
        trigger_device = dev_ids[ID_TRIGGER];
        while(1){
            if (e_flag) break;
            poll(&pfds[trigger_device], 1, -1); //Polling the trigger device only.
            if(pfds[trigger_device].revents){
                if(pfds[trigger_device].revents & POLLIN){
                    res = read(pfds[trigger_device].fd, &event, sizeof(event));
                    if(res < (int)sizeof(event)){
                        print_err("Couldn't get event for trigger device.\n", NULL, 0, print_flags);
                        return 1;
                    }

                    if (event.code == trigger_event)
                        e_flag = 1;

                    if (print_flags & PRINT_ALL)
                        print_event(&event, trigger_device);
                }
            }
        }
        print_all("Stop trigger : listening finished.\n", NULL, 0, print_flags);
    }

    print_all("Releasing device...\n", NULL, 0, print_flags);
    ioctl(pfds[blocking_device].fd, EVIOCGRAB, 0);
    close_devices(print_flags);
    print_all("Exiting...\n", NULL, 0, print_flags);
    return 0;
}

static void sig_handler(int signum)
{
    e_flag = 1;
}