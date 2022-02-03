/*
blockevent.c - blockevent for Android v0.2.0

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

#include <stdio.h> //fprintf, stderr, EOF
#include <stdlib.h> //exit
#include <unistd.h> //opterr, getopt, optopt, pause, close, optarg
#include <ctype.h> //isprint
#include <fcntl.h> //open
#include <linux/input.h> //ioctl, EVIOCGRAB, EVIOCGNAME
#include <signal.h> //raise
#include <stdint.h> //uint8_t
#include <dirent.h> //DIR
#include <sys/limits.h> //PATH_MAX
#include <string.h> //strlen

#define VERSION_MAJOR "0"
#define VERSION_MINOR "2"
#define VERSION_PATCH "0"

#define test_bit(bit, array) (array[bit / 8] & (1 << (bit % 8)))

static char *device_to_block;

static int find_touch_device(const char *device)
{
    int fd;
    uint8_t abs_mask[ABS_MAX/8 + 1];
    uint8_t key_mask[KEY_MAX/8 + 1];

    fd = open(device, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "could not open %s\n", device);
        return -1;
    }

    if ((ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_mask)), abs_mask) < 1) ||
        (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_mask)), key_mask) < 1)){
            return -1;
            close(fd);
    }

    /*
    I followed here to get capabilities of devices.
    https://www.linuxjournal.com/article/6429

    I followed here to classify devices with using their capabilities.
    https://source.android.com/devices/input/touch-devices
    */
    if ((test_bit(ABS_MT_POSITION_X, abs_mask) && test_bit(ABS_MT_POSITION_Y, abs_mask)) 
        || (test_bit(ABS_X, abs_mask) && test_bit(ABS_Y, abs_mask))){
            if (test_bit(BTN_TOUCH, key_mask)){
                printf("Touch device detected\n");
                device_to_block = malloc(sizeof(char) * sizeof(*device)); 
                strcpy(device_to_block, device);
                close(fd);
                return 0;
            }
    }

    close(fd);
    return 1;
}

static int scan_dir(const char *dirname)
{
    char devname[PATH_MAX];
    char *filename;
    struct dirent *de;
    device_to_block = NULL;
    DIR *dir = opendir(dirname);
    if (dir == NULL)
        return -1;
    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    while ((de = readdir(dir))){
        if (de->d_name[0] != 'e')
            continue;
        strcpy(filename, de->d_name);
        if (find_touch_device(devname) == 0) break;
    }
    closedir(dir);
    return 0;
}

static void usage(char *name)
{
    fprintf(stderr, "Usage: %s [-b] [-t] [-d input device]\n", name);
    fprintf(stderr, "      : run as normal. To stop use 'CTRL+C' or 'pkill -2 blockevent'\n");
    fprintf(stderr, "    -b: run as a background job. To stop use 'kill \"%%$(pgrep blockevent)\"'\n");
    fprintf(stderr, "    -t: block touch screen.\n");
    fprintf(stderr, "    -d: block input device.'/dev/input/eventX'. Use 'getevent' to find eventX.\n");
    fprintf(stderr, "    -h: print help.\n");
    fprintf(stderr, "\nblockevent for Android v%s.%s.%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    fprintf(stderr, "https://github.com/nmelihsensoy/blockevent\n");
}

int main(int argc, char *argv[])
{
    int c;
    int fd;
    short ts_find = 0;
    short bg_switch = 0;
    char input_name[80] = "Unknown";
    const char *device_path = "/dev/input";

    opterr = 0;
    do{
        c = getopt (argc, argv, "bthd:");
        if (c == EOF)
            break;
        switch (c){
        case 'b':
            bg_switch = 1;
            break;
        case 't':
            ts_find = 1;
            break;
        case 'd':
            device_to_block = optarg;
            break;
        case '?':
            if (optopt == 'd')
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

    if (ts_find == 1)
        scan_dir(device_path);
    
    fd = open(device_to_block, O_RDONLY | O_NONBLOCK); 
    if (fd == -1){
        fprintf(stderr, "failed to open '%s'", device_to_block);
        exit(1);
    }

    ioctl(fd, EVIOCGNAME(sizeof(input_name)), input_name);
    //i couldn't get any stdout with printf.
    fprintf(stderr, "getting exclusive access for: %s", input_name);	
    
    /* Grabbing */
    if (ioctl(fd, EVIOCGRAB, 1) < 0){
        fprintf(stderr, "couldn't get exclusive access for: %s", input_name);
        close(fd);
        exit(1);
    }

    /* Waiting for a signal. */
    if (bg_switch == 0){
        pause();
    }

    /* Changing itself to a bg process. */
    if (bg_switch == 1){
        raise(SIGTSTP);
    }
    
    /* Releasing */
    ioctl(fd, EVIOCGRAB, 0);
    close(fd);

    return 0;
}