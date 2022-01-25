#include <stdio.h> //fprintf, stderr, EOF
#include <stdlib.h> //exit
#include <unistd.h> //opterr, getopt, optopt
#include <ctype.h> //isprint

static void usage(char *name)
{
    fprintf(stderr, "Usage: %s [-r] [device]\n", name);
    fprintf(stderr, "      : disable device temporarily.\n");
    fprintf(stderr, "    -r: disable device until reboot.\n");
}

int main(int argc, char *argv[])
{
    int c;
    const char *device = NULL;

    opterr = 0;
    do{
        c = getopt (argc, argv, "rh::");
        if (c == EOF)
            break;
        switch (c){
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
    /* accepting only one device name for optionless. */
    if (optind != argc){
        usage(argv[0]);
        exit(1);
    }
    /* device name check. */
    if (device == NULL){
        usage(argv[0]);
        exit(1);
    }   
    
    return 0;
}