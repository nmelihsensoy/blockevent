/*
blockevent.c - blockevent for Android v0.1.0

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
#include <unistd.h> //opterr, getopt, optopt, pause, close 
#include <ctype.h> //isprint
#include <fcntl.h> //open
#include <linux/input.h> //ioctl, EVIOCGRAB, EVIOCGNAME
#include <signal.h> //raise

#define VERSION_MAJOR "0"
#define VERSION_MINOR "1"
#define VERSION_PATCH "0"

static void usage(char *name)
{
    fprintf(stderr, "Usage: %s [-b] [input device]\n", name);
    fprintf(stderr, "                : run as normal. To stop use 'CTRL+C' or 'pkill -2 blockevent'\n");
    fprintf(stderr, "              -b: run as a background job. To stop use 'kill \"%%$(pgrep blockevent)\"'\n");
    fprintf(stderr, "              -h: print help.\n");
    fprintf(stderr, "    input device: '/dev/input/eventX' find X with using getevent tool.\n");
    fprintf(stderr, "\nblockevent for Android v%s.%s.%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    fprintf(stderr, "https://github.com/nmelihsensoy/blockevent\n");
}

int main(int argc, char *argv[])
{
    int c;
    int touch_fd;
    short bg_switch = 0;
    const char *device = NULL;
    char input_name[80] = "Unknown";

    opterr = 0;
    do{
        c = getopt (argc, argv, "bh::");
        if (c == EOF)
            break;
        switch (c){
        case 'b':
            bg_switch = 1;
            break;
        case '?':
            if (isprint(optopt))
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
            fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
            return 1;
        case 'h':
            usage(argv[0]);
            exit(1);
        }
    }while(1);
    /* getting device name without using any option. */
    if (optind + 1 == argc){
        device = argv[optind];
        optind++;
    }
    /* accepting only one device name. */
    if (optind != argc){
        usage(argv[0]);
        exit(1);
    }
    /* device name check. */
    if (device == NULL){
        usage(argv[0]);
        exit(1);
    } 

    touch_fd = open(device, O_RDONLY | O_NONBLOCK);
	
	if(touch_fd == -1){
		printf("failed to open '%s'", device);
		exit(1);
	}
	
	ioctl(touch_fd, EVIOCGNAME(sizeof(input_name)), input_name);
	//i couldn't get any stdout with printf.
	fprintf(stderr, "getting exclusive access for: %s", input_name);	
    
    if(ioctl(touch_fd, EVIOCGRAB, 1) < 0){
        printf("couldn't get exclusive access for: %s", input_name);
        close(touch_fd);
        exit(1);
    }

    if(bg_switch == 0){
        pause();
    }

    if(bg_switch == 1){
        raise(SIGTSTP);
    }
    
    ioctl(touch_fd, EVIOCGRAB, 0);
    close(touch_fd);

    return 0;
}