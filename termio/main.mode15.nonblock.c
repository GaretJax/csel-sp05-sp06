#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>


#define CHECKERR(val, fmt, ...) {               \
    if ((val) < 0) {                            \
        fprintf(stderr, fmt": ", __VA_ARGS__);  \
        perror(NULL);                           \
        exit(EXIT_FAILURE);                     \
    }                                           \
}

#define BUFSIZE 51

#define TIMEOUT 620
#define INTRA_TIMEOUT 20



static struct termios saved_conf;
static int conf_was_saved = 0;
static int stop = 0;


void configure(int fd)
{
    struct termios conf;

    printf("Configuring through termios...\n");

    // Zero out the struct
    memset(&conf, 0, sizeof(conf));

    // Configure flags
    conf.c_iflag = IGNCR;
    conf.c_oflag = 0;
    conf.c_cflag = CS8 | CREAD;
    conf.c_lflag = ICANON;

    // Set speed
    CHECKERR(cfsetispeed(&conf, B9600), "Couldn't set in speed (fd=%d)", fd);
    CHECKERR(cfsetospeed(&conf, B9600), "Couldn't set out speed (fd=%d)", fd);

    // Save previous termios config and apply the new one
    CHECKERR(tcgetattr(fd, &saved_conf), "Couldn't save termios (fd=%d)", fd);
    conf_was_saved = 1;
    CHECKERR(tcsetattr(fd, TCSAFLUSH, &conf), "Couldn't set termios (fd=%d)", fd);
}


int open_and_setup(char * device)
{
    int fd;

    printf("Opening device at '%s'...\n", device);
    fd = open(device, O_RDWR | O_NONBLOCK);
    CHECKERR(fd, "Failed to open device %s", device);

    configure(fd);

    return fd;
}


void close_and_restore(int fd)
{
    if (conf_was_saved) {
        CHECKERR(tcsetattr(fd, TCSAFLUSH, &saved_conf), "Couldn't reset termios for fd=%d", fd);
    }
    close(fd);
}


void repr(char * str)
{
    printf("\"");
    while (*str) {
        switch (*str) {
            case 9: printf("\\t"); break;
            case 10: printf("\\n"); break;
            case 13: printf("\\r"); break;
            default:
                if (*str < 32) {
                    printf("\\x%2d", *str);
                } else {
                    printf("%c", *str);
                }
        }
        str++;
    }
    printf("\"\n");
}


void trim(char *str)
{
    char *start = str;
    char *end = NULL;

    if (str == NULL) {
        return;
    }

    if (str[0] == '\0') {
        return;
    }

    end = str + strlen(str) - 1;

    while (isspace(*end)) {
        end--;
    }
    while (isspace(*start) && start < end) {
        start++;
    }
    end++;
    *end = '\0';

    if (start != str) {
        while (*start) {
            *str++ = *start++;
        }
        *str = '\0';
    }
}


void cleanup()
{
    if (stop == 0) {
        printf("Caught SIGINT...\n");
        stop = 1;
    }
}


int main(int argc, char ** argv)
{
    char buf[BUFSIZE];
    char * device;
    ssize_t size;
    unsigned int sensor;
    unsigned int measure;
    double value;
    int fd;
    int slept_time;
    int readsize;
    struct sigaction sa;

    if (argc != 2) {
        printf("Usage: %s DEVICE-PATH\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    device = argv[1];

    fd = open_and_setup(device);

    sigemptyset(&sa.sa_mask);
    sa.sa_handler = cleanup;
    sigaction(SIGINT, &sa, NULL);

    printf("Waiting for data...\n");

    while (stop == 0) {
read:
        size = write(fd, "get", 3);
        if (stop == 0) {
            CHECKERR(size, "Failed to write data to %s", device);
        } else {
            printf("Write interrupted, exiting main loop...\n");
            goto cleanup;
        }
        printf(" > %d bytes written\n", size);

        readsize = 0;
        slept_time = 0;

        while (1) {
            size = read(fd, &buf[readsize], BUFSIZE - 1 - readsize);
            if (stop == 0 && size == -1 && errno == EAGAIN) {
                if (slept_time >= TIMEOUT) {
                    printf(" - Read operation timed out (%dms), restarting read loop...\n", slept_time);
                    goto read;
                }
                usleep(INTRA_TIMEOUT * 1000);
                slept_time += INTRA_TIMEOUT;
                continue;
            } else if (stop == 0) {
                CHECKERR(size, "Failed to read data from %s", device);
            } else {
                printf("Read interrupted, exiting main loop...\n");
                goto cleanup;
            }

            readsize += size;
            buf[readsize] = '\0';

            if (strchr(&buf[readsize - 1], '\n')) {
                // We found a line feed (CRs are ignored by termios)
                // Note: If there is some data after the newline, it will
                // be discarded. That should not be a problem because
                // no more data should be received before we send the
                // 'get' string one more time.
                goto process;
            } else if (readsize >= BUFSIZE - 1) {
                printf(" - Received data is too long (%d), restarting read loop...\n", readsize);
                goto read;
            }
        }

process:
        if (sscanf(buf, "StringFromSensor%u_%u_%lf\n", &sensor, &measure, &value) != 3) {
            printf(" - Received data has wrong format. Received string (%d bytes) is: ", readsize);
            repr(buf);
        } else {
            printf(" + New value received from sensor: %u %u %08.3f\n", sensor, measure, value);
        }
    }

cleanup:
    printf("Main loop done, cleaning up...\n");
    close_and_restore(fd);

    return EXIT_SUCCESS;
}
