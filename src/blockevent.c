/*
blockevent.c - blockevent for Android v0.4.6

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
#include <linux/uinput.h>

#define VERSION "0.4.6"
#define DEV_DIR "/dev/input"
#define VIRTUAL_DEV_LOC "/dev/uinput"
#define VIRTUAL_DEV_NAME "blockevent-"VERSION"_virtual_dev"
#define PRESET_DEV_CNT 4

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point left_bottom;
    Point right_top;
} Rect;

static int nfds;
static struct pollfd *pfds;
static char **devs;
static int dev_nid[] = {-1, -1, -1, -1, -1};
static int uinp_fd;
static struct uinput_user_dev uinp;

volatile sig_atomic_t e_flag = 0;
static void sig_handler(int signum);

typedef enum {
    DEV_TOUCHSCREEN      = 1U << 0,
    DEV_VOLUP            = 1U << 1,
    DEV_VOLDOWN          = 1U << 2,
    DEV_ANY              = 1U << 3,
    DEV_POWERBTN         = 1U << 4,
    ID_TOUCHSCREEN       = 1,  
    ID_VOLUP             = 2,
    ID_VOLDOWN           = 3,
    ID_POWER             = 4,
} dev_types;

enum {
    ST_SCAN              = 1U << 0,
    ST_TRIGGER           = 1U << 1,
    ST_CUSTOM_TRIGGER    = 1U << 2,
    ST_TS_BLOCK_PARTLY   = 1U << 3,
    ST_REVERSE_BLOCK     = 1U << 4,
    ST_VIRT_BUTTON       = 1U << 5,
    PRINT_ERR            = 1U << 0,
    PRINT_NONE           = 1U << 1,
    PRINT_ALL            = 1U << 2,
    PRINT_ISSET          = 1U << 3,
};

static const struct dev_preset {
	dev_types dev_type;
    dev_types dev_id;
    union{
       struct input_event event;
       Rect *area;
    };
    
} dev_presets[] = {
    { DEV_TOUCHSCREEN, ID_TOUCHSCREEN, .area=NULL},
	{ DEV_VOLDOWN, ID_VOLDOWN, {0, 0, EV_KEY, KEY_VOLUMEDOWN, 1}},
    { DEV_VOLUP, ID_VOLUP, {0, 0, EV_KEY, KEY_VOLUMEUP, 1}},
    { DEV_POWERBTN, ID_POWER, {0, 0, EV_KEY, KEY_POWER, 1}},
};

static inline bool test_bit(unsigned bit, unsigned char *array)
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
    fprintf(stderr, "[INFO] dev: %s Event: Type[%d] Code[%d] Value[%d]\n", 
        devs[dev_id], ev->type, ev->code, ev->value);
}

//displayX = (x - minX) * displayWidth / (maxX - minX + 1) : calculation from AOSP docs.
static inline int raw_to_pixel(int raw_point, int min, int max, int axis_length)
{
    return (raw_point - min) * axis_length / (max - min + 1);
}

static inline int pixel_to_raw(int pixel, int min, int max, int axis_length)
{
    return (pixel * (max - min + 1) / axis_length ) + min;
}

static inline bool are_same_event(struct input_event *ev1, struct input_event *ev2)
{
    return ((ev1->code == ev2->code) && (ev1->type == ev2->type) && (ev1->value == ev2->value));
}

static inline bool is_in_area(Point *p1, Rect *r1)
{
   return (p1->x >= r1->left_bottom.x
        && p1->y <= r1->left_bottom.y
        && p1->x <= r1->right_top.x
        && p1->y >= r1->right_top.y);
}

static inline void set_point_from_event(Point *p, struct input_event *ev)
{
    if (ev->code == ABS_MT_POSITION_X)
        p->x = ev->value;
    if (ev->code == ABS_MT_POSITION_Y)
        p->y = ev->value;
}

static inline bool detect_double_tap(int64_t *now, int64_t *prev, struct input_event *ev, u_int32_t timeout)
{
    *now = ev->time.tv_sec * 1000000LL + ev->time.tv_usec; // current time in microseconds. 
    if (*now - *prev < timeout) 
        return true; 
    *prev = *now;
    return false;
}

static int parse_pixel_rect(char *raw_text, int **return_coords, char *del, int width, int height, uint8_t print_flags)
{   
    int i = 0;
    char *tmp = NULL;
    raw_text = strtok(raw_text, del); // x1,y1,x2,y2
    while (raw_text != NULL && i < 4) {
        *return_coords[i] = (int)strtol(raw_text, &tmp, 0);
        if (tmp == raw_text){
            print_err("[ERR] '%s' not a decimal number\n", raw_text, 0, print_flags);
            return -1;
        }

        if ((i & 1) != 0) // = !isEven
            *return_coords[i] = pixel_to_raw(*return_coords[i], 0, width, width);
        else
            *return_coords[i] = pixel_to_raw(*return_coords[i], 0, height, height);
        
        raw_text = strtok(NULL, del);
        i++;
    }
    if (i != 4) 
        return -1;
    
    return 0;
}

static int clone_device(int dev_id, uint8_t dev_type, uint8_t print_flags)
{   
    int i;
    uint8_t abs_mask[ABS_CNT / 8];
    uint8_t key_mask[KEY_CNT / 8 ];
    uint8_t prop_mask[INPUT_PROP_CNT / 8];
    struct input_absinfo abs_limit;
    char str[100];

    if (nfds < dev_id || pfds[dev_id].fd < 0){
        print_err("[ERR] no device found to clone.\n", NULL, 0, print_flags);
        return -1;
    }

    if (dev_type == 0){
        print_err("[ERR] type for clone device is not specified.\n", NULL, 0, print_flags);
        return -1;
    }

    uinp_fd = open(VIRTUAL_DEV_LOC, O_WRONLY|O_NONBLOCK);
    if(uinp_fd < 0) {
        print_err("[ERR] Unable to open /dev/uinput\n", NULL, 0, print_flags);
        return -1;
    }

    memset(&uinp, 0, sizeof(uinp));
    strncpy(uinp.name, VIRTUAL_DEV_NAME, UINPUT_MAX_NAME_SIZE);
    uinp.id.version = 4;
    uinp.id.bustype = BUS_VIRTUAL;
    
    if (dev_type & DEV_TOUCHSCREEN){
        ioctl(uinp_fd, UI_SET_EVBIT, EV_KEY);
        ioctl(uinp_fd, UI_SET_EVBIT, EV_ABS);
        ioctl(uinp_fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);
        ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TOUCH);
        
        ioctl(pfds[dev_id].fd, EVIOCGBIT(EV_ABS, sizeof(abs_mask)), abs_mask);

        for (i = 0; i < ABS_MAX; i++) {
            if (test_bit(i, abs_mask)) {
                ioctl(pfds[dev_id].fd, EVIOCGABS(i), &abs_limit);

                uinp.absmin[i] = abs_limit.minimum;
                uinp.absmax[i] = abs_limit.maximum;
                ioctl(uinp_fd, UI_SET_ABSBIT, i);
            }
        }
    }

    memset(str, 0, sizeof(str));
    sprintf(str, "%d", uinp_fd);

    write(uinp_fd, &uinp, sizeof(uinp));
    if (ioctl(uinp_fd, UI_DEV_CREATE) < 0){
        print_err("[ERR] An error occured when creating the '%s'.\n", VIRTUAL_DEV_NAME, 0, print_flags);
        return -1;
    }
    
    print_all("[INFO] '%s' created successfully. \n", VIRTUAL_DEV_NAME, 0, print_flags);
    usleep(10000);

    return 0;
}

static int open_device(const char *device, uint8_t *classes, uint8_t print_flags, bool return_dev)
{
    int i, fd;
    uint8_t abs_mask[ABS_CNT / 8];
    uint8_t key_mask[KEY_CNT / 8 ];
    uint8_t prop_mask[INPUT_PROP_CNT / 8];
    uint8_t is_recognized = 0;
    uint8_t dev_type = 0;
    char name[80] = "?";

    for (i = 0; i < nfds; i++) {
        if (strcmp(device, devs[i]) == 0){
            print_err("[ERR] '%s' device is already open.", device, 0, print_flags);
            if (return_dev) return i;
            else return -1;
        }
    }

    print_all("[INFO] '%s' is opening...\n", device, 0, print_flags);
    fd = open(device, O_RDONLY | O_NONBLOCK);
    if(fd < 0) {
        print_err("[ERR] '%s' could not open. \n", device, 0, print_flags);
        return -1;
    }

    ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_mask)), abs_mask);
    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_mask)), key_mask);
    ioctl(fd, EVIOCGPROP(sizeof(prop_mask)), prop_mask);

    if ((is_recognized == 0) && (*classes & DEV_ANY)){
        print_all("[INFO] 'DEV_ANY' detected.\n", NULL, 0, print_flags);
        is_recognized |= 1;
        *classes &= ~DEV_ANY;
    }

    // power button detection shortcut for qualcomm devices
    if ((is_recognized == 0) && (*classes & DEV_POWERBTN)){
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        if (strcmp(name, "qpnp_pon") == 0){
            print_all("[INFO] Qualcomm device'DEV_POWERBTN' detected...\n", NULL, 0, print_flags);
            is_recognized |= 1;
            dev_type = ID_POWER;
            *classes &= ~DEV_POWERBTN;
        }
    }

    /*
    I followed here to get capabilities of devices.
    https://www.linuxjournal.com/article/6429

    I followed here to classify devices with using their capabilities.
    https://source.android.com/devices/input/touch-devices
    */
    if ((is_recognized == 0) && (*classes & DEV_TOUCHSCREEN)){
        if ((test_bit(ABS_MT_POSITION_X, abs_mask) && test_bit(ABS_MT_POSITION_Y, abs_mask)) 
            || (test_bit(ABS_X, abs_mask) && test_bit(ABS_Y, abs_mask))){
                if (test_bit(BTN_TOUCH, key_mask) && test_bit(INPUT_PROP_DIRECT, prop_mask)){
                    print_all("[INFO] 'DEV_TOUCHSCREEN' detected...\n", NULL, 0, print_flags);
                    is_recognized |= 1;
                    dev_type = ID_TOUCHSCREEN;
                    *classes &= ~DEV_TOUCHSCREEN;
                }
        }  
    }

    if ((is_recognized == 0) && (*classes & DEV_VOLDOWN) && test_bit(KEY_VOLUMEDOWN, key_mask)){
        if (!test_bit(KEY_MEDIA, key_mask)){
            print_all("[INFO] 'DEV_VOLDOWN' detected...\n", NULL, 0, print_flags);
            is_recognized |= 1;
            dev_type = ID_VOLDOWN;
            *classes &= ~DEV_VOLDOWN;
        }
    }
    if ((is_recognized == 0) && (*classes & DEV_VOLUP) && test_bit(KEY_VOLUMEUP, key_mask)){
        if (!test_bit(KEY_MEDIA, key_mask)){
            print_all("[INFO] 'DEV_VOLUP' detected...\n", NULL, 0, print_flags);
            is_recognized |= 1;
            dev_type = ID_VOLUP;
            *classes &= ~DEV_VOLUP;
        }
    }
    if ((is_recognized == 0) && (*classes & DEV_POWERBTN) && test_bit(KEY_POWER, key_mask)){
        print_all("[INFO] 'DEV_POWERBTN' detected...\n", NULL, 0, print_flags);
        is_recognized |= 1;
        dev_type = ID_POWER;
        *classes &= ~DEV_POWERBTN;
    }

    if (is_recognized & 1){
        pfds = realloc(pfds, sizeof(struct pollfd) * (nfds + 1));
        devs = realloc(devs, sizeof(char *) * (nfds + 1));
        pfds[nfds].fd = fd;
        pfds[nfds].events = POLLIN;
        devs[nfds] = strdup(device);
        dev_nid[dev_type] = nfds;

        nfds++;
        if (return_dev) return (nfds-1);
        else return 0;
    }

    print_all("[INFO] Unknown device.Closing...\n\n", NULL, 0, print_flags);
    close(fd);
    return -1;
}

static void close_devices(uint8_t print_flags)
{
    nfds--;
    while(nfds >= 0) {
        print_all("[INFO] '%s' closing...\n", devs[nfds], 0, print_flags);
        ioctl(pfds[nfds].fd, EVIOCGRAB, 0);
        close(pfds[nfds].fd);
        free(devs[nfds]);
        nfds--;
    }
    if (uinp_fd >= 0){
        usleep(10000);
        ioctl(uinp_fd, UI_DEV_DESTROY);
        close(uinp_fd);
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

        snprintf(devloc, sizeof(devloc), "%s/%s", DEV_DIR, devname);

        if (open_device(devloc, &classes, print_flags, false) == 0)
            temp &= temp - 1; //See brian kernighan's bit counting algorithm.
        if (temp == 0) break;
    }
    free(devname);
    closedir(dir);

    if (temp != 0 && classes != 0) 
        return temp;

    return 0;
}

static void usage(char *name)
{
    fprintf(stderr, "Usage: %s -d device... [-s trigger] [-v level] [-r x1,y1,x2,y2...] [-R ] [-W width] [-H height]\n", name);
    fprintf(stderr, "    -d: blocking device. Preset device id or a path specified device.\n");
    fprintf(stderr, "        Preset devices (0=Touchscreen, 1=Volume Down, 2=Volume Up, 3=Power Button)\n");
    fprintf(stderr, "        Specific device (/dev/input/eventX) \n\n");
    fprintf(stderr, "    -s: stop trigger.Preset device or a device specified event.\n");
    fprintf(stderr, "        Preset devices (0=Area Button, 1=Volume Down, 2=Volume Up, 3=Power Button)\n");
    fprintf(stderr, "        Specific event (/dev/input/eventX:<Type>,<Code>,<Value>)\n\n");
    fprintf(stderr, "    -v: verbosity level.(Errors=1, None=2, All=4) (Default=1)\n");
    fprintf(stderr, "    -r: rectangle.Comma seperated left bottom and right top corner point coordinates.\n");
    fprintf(stderr, "        -d 0 -s (1-3) -r x1,y1,x2,y2 : specifies partly blocking rectangle on the touchscreen.\n");
    fprintf(stderr, "        -d 0 -r x1,y1,x2,y2 -s 0 -r x1,y1,x2,y2 : specifies partly blocking.\n");
    fprintf(stderr, "        -d (1-3) -s 0 -r x1,y1,x2,y2 : specifies area button's rectangle.\n\n");
    fprintf(stderr, "    -R: reverse. Reverses blocking rectangle on the touchscreen.\n");
    fprintf(stderr, "    -W: screen width.\n");
    fprintf(stderr, "    -H: screen height.\n");
    fprintf(stderr, "    -h: print help.\n");
    fprintf(stderr, "\nblockevent for Android v%s\n", VERSION);
    fprintf(stderr, "https://github.com/nmelihsensoy/blockevent\n");
}

int main(int argc, char *argv[])
{   
    int res;
    int c, i;
    Rect area;
    int n_devs = 0;
    int64_t prev, now;
    uint8_t flags = 0;
    Point touch_point;
    char *tmp_end_ptr;
    Rect v_button_area;
    uint8_t classes = 0;
    int screen_width = 0; 
    int screen_height = 0; 
    uint8_t tmp_flags = 0;
    char *tmp_area = NULL;
    char *tmp_event = NULL;
    char *given_devices[10]; // max 10 device
    uint8_t print_flags = 0;
    int trigger_dev_id = -1;
    struct input_event event;
    char *trigger_dev = NULL;
    bool temp_reverse = false;
    char *tmp_v_button_area = NULL;
    struct input_event trigger_event;
    int *area_coords[] = {&area.left_bottom.x, &area.left_bottom.y, 
                            &area.right_top.x, &area.right_top.y};
    int *v_button_coords[] = {&v_button_area.left_bottom.x, &v_button_area.left_bottom.y, 
                            &v_button_area.right_top.x, &v_button_area.right_top.y};    

    i = 0;
    opterr = 0;
    do{
        c = getopt (argc, argv, "hd:s:v:W:H:Rr:");
        if (c == EOF)
            break;
        switch (c){
        case 'd':
            given_devices[n_devs] = optarg;
            n_devs++;
            break;
        case 's':
            trigger_dev = optarg;
            flags |= ST_TRIGGER;
            break;
        case 'v':
            print_flags |= strtoul(optarg, NULL, 0);
            print_flags |= PRINT_ISSET;
            break;
        case 'W':
            screen_width = (int)strtol(optarg, &tmp_end_ptr, 0);
            if (tmp_end_ptr == optarg){
                usage(argv[0]);
                exit(1);
            }
            break;
        case 'H':
            screen_height = (int)strtol(optarg, &tmp_end_ptr, 0);
            if (tmp_end_ptr == optarg){
                usage(argv[0]);
                exit(1);
            }
            break;
        case 'r':
            if (i == 0){
                flags |= ST_TS_BLOCK_PARTLY;
                tmp_area = optarg;
                i++;
            }else{
                tmp_v_button_area = optarg;
            }       
            break;
        case 'R':
            flags |= ST_REVERSE_BLOCK;
            break;
        case '?':
            if (optopt != 'R' || optopt != 'h') 
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

    // set default verbose level to PRINT_ERR
    if ((print_flags & PRINT_ISSET) == 0)
        print_flags |= PRINT_ERR;

    tmp_end_ptr = NULL;
    nfds = 0;
    signal(SIGINT, sig_handler);

    if (flags & ST_TRIGGER){
        if (trigger_dev == NULL){
            return 1;
        }

        i = trigger_dev[0] - '0';
        if (i == 0 && trigger_dev[1] == '\0'){
            flags |= ST_VIRT_BUTTON;
            if (tmp_v_button_area == NULL && tmp_area != NULL){ // -d 0 -s 0 -r x,y,x,y
                flags &= ~ST_TS_BLOCK_PARTLY;
                if (parse_pixel_rect(tmp_area, v_button_coords, ",", screen_width, screen_height, print_flags) < 0) 
                    return 1;
            }else{
                if (parse_pixel_rect(tmp_v_button_area, v_button_coords, ",", screen_width, screen_height, print_flags) < 0)
                    return 1;
            }
        }else if (i > 0 && i <= PRESET_DEV_CNT){
            flags |= ST_SCAN;
            classes |= dev_presets[i].dev_type;
            trigger_dev_id = dev_presets[i].dev_id;
            trigger_event = dev_presets[i].event;
        }else{
            flags |= ST_CUSTOM_TRIGGER;
            trigger_dev = strtok(trigger_dev, ":"); // eventX:1,114,1 # get eventX
            tmp_event = strtok(NULL, ":"); // eventX:1,114,1 # get 1,114,1
            if (tmp_event == NULL){
                print_err("[ERR] it's mandatory to providing an event.\n", NULL, 0, print_flags);
                return 1;
            }
            tmp_event = strtok(tmp_event, ","); // parse event

            i=0;
            while (tmp_event != NULL) {
                if (i == 0){
                    trigger_event.type = strtol(tmp_event, NULL, 0);
                }else if(i == 1){
                    trigger_event.code = strtol(tmp_event, NULL, 0);
                }else if(i==2){
                    trigger_event.value = strtol(tmp_event, NULL, 0);
                }
                tmp_event = strtok(NULL, ",");
                i++;
            }
        }
    }

    for (i = 0; i < n_devs; i++){
        res = given_devices[i][0] - '0';
        if (res >= 0 && res <= PRESET_DEV_CNT){
            flags |= ST_SCAN;
            classes |= dev_presets[res].dev_type;
            print_all("[INFO] device added to the scanner.\n", NULL, i, print_flags);
        }else{
            tmp_flags = 0;
            tmp_flags |= DEV_ANY;
            res = open_device(given_devices[i], &tmp_flags, print_flags, false);
            if (res < 0){
                return 1;
            }
        }
    }
    
    if (flags & ST_VIRT_BUTTON){
        if ((flags & DEV_TOUCHSCREEN) == 0 || screen_height == 0 || screen_width == 0){
            usage(argv[0]);
            return 1;
        }
    }

    if (flags & ST_TS_BLOCK_PARTLY){
        if ((flags & DEV_TOUCHSCREEN) == 0 || screen_height == 0 || screen_width == 0){
            usage(argv[0]);
            return 1;
        }

       if (parse_pixel_rect(tmp_area, area_coords, ",", screen_width, screen_height, print_flags) < 0){
           usage(argv[0]);
           return 1;
       }
    }

    // open manually entered device directly with 'DEV_ANY' flag and store it's pfds id
    if (flags & ST_CUSTOM_TRIGGER){
        tmp_flags = 0;
        tmp_flags |= DEV_ANY;
        trigger_dev_id = open_device(trigger_dev, &tmp_flags, print_flags, true);
        if (trigger_dev_id < 0) 
            return 1;
    }

    // start scanning for preset devices
    if (flags & ST_SCAN){
        print_all("[INFO] Device scan has started with flag:'%lu'\n", NULL, flags, print_flags);
        res = scan_devices(classes, print_flags);
        if (res < 0){
            print_err("[ERR] Failed to open '%s'.\n", DEV_DIR, 0, print_flags);
            return 1;
        }else if(res > 0){
            print_err("[ERR] Scan couldn't find all devices. Mising device flag:'%d'.\n", NULL, res, print_flags);
            return 1;
        }
    }

    trigger_dev_id = dev_nid[trigger_dev_id]; //real pfds id is getting after the scan

    for (i = 0; i < nfds; i++){   
        if ((i == trigger_dev_id) && (flags & ST_CUSTOM_TRIGGER) == 0)
            continue;

        if (ioctl(pfds[i].fd, EVIOCGRAB, 1) < 0){
            print_err("[ERR] '%s' couldn't get exclusive access.Aborted.\n", devs[i], 0, print_flags);
            close_devices(print_flags);
            return 1;
        }
    }

    if (flags & ST_TS_BLOCK_PARTLY){
        if (clone_device(dev_nid[ID_TOUCHSCREEN], DEV_TOUCHSCREEN, print_flags) < 0){
            close_devices(print_flags);
            return 1;
        }
    }

    if ((flags & ST_TRIGGER) == 0 && (flags & ST_TS_BLOCK_PARTLY) == 0 && (flags & ST_VIRT_BUTTON) == 0){
        print_all("[INFO] (CTRL+C) to stop blocking.\n", NULL, 0, print_flags);
        pause();
    }

    while(1){
        if (e_flag) break;
        poll(pfds, nfds, -1);

        for (i = 0; i < nfds; i++){
            if (pfds[i].revents){
                if(pfds[i].revents & POLLIN) {
                    res = read(pfds[i].fd, &event, sizeof(event));
                    if(res < (int)sizeof(event)){
                        print_err("[ERR] '%s' couldn't get event.\n", devs[i], 0, print_flags);
                        e_flag = 1;
                    }

                    if (print_flags & PRINT_ALL) 
                        print_event(&event, i);

                    if ((flags & ST_TS_BLOCK_PARTLY) && i == dev_nid[ID_TOUCHSCREEN]){
                        set_point_from_event(&touch_point, &event);

                        temp_reverse = is_in_area(&touch_point, &area);
                        if (flags & ST_REVERSE_BLOCK) 
                            temp_reverse = !temp_reverse;

                        if (temp_reverse)
                            continue;

                        write(uinp_fd, &event, sizeof(event));    
                        if (event.type == EV_SYN)
                            usleep(1000);
                    }
                    
                    if((flags & ST_VIRT_BUTTON) && i == dev_nid[ID_TOUCHSCREEN]){
                        set_point_from_event(&touch_point, &event);

                        if (event.code == BTN_TOUCH 
                            && is_in_area(&touch_point, &v_button_area) 
                            && event.value == 1
                            && detect_double_tap(&now, &prev, &event, 200000)) // 200 miliseconds
                                e_flag = 1;
                    }

                    if ((flags & ST_TRIGGER) && are_same_event(&trigger_event, &event)) 
                        e_flag = 1;
                }
            }
        }
    }

    close_devices(print_flags);
    return 0;
}

static void sig_handler(int signum)
{
    e_flag = 1;
}