#define _GNU_SOURCE 1
#define _POSIX_SOURCE 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/errno.h>

#include <signal.h>
#include <sys/time.h>


#define CHECKERR(val, fmt) {                    \
    if ((val) < 0) {                            \
        fprintf(stderr, fmt": ");               \
        perror(NULL);                           \
        exit(EXIT_FAILURE);                     \
    }                                           \
}


// Thents of second (1 = 100ms, 10 = 1s)
#define SPEED 5
#define SLEEPFUNC sleepfor

/**
 * 7-segment display and LED interface
 */
struct gpio_ctrl {
    uint16_t reserved1[(0x08-0x00)/2];
    uint16_t seg7_rw;
    uint16_t seg7_ctrl;
    uint16_t seg7_id;
    uint16_t reserved2[1];
    uint16_t leds_rw;
    uint16_t leds_ctrl;
    uint16_t leds_id;
};

static volatile struct gpio_ctrl* gpio = 0;

/* 7-segment: segment definition

   +-- seg A --+
   |           |
   seg F       seg B
   |           |
   +-- seg G --+
   |           |
   seg E       seg C
   |           |
   +-- seg D --+
*/

#define SEG_DOT 0x004
#define SEG_A 0x008
#define SEG_B 0x010
#define SEG_C 0x020
#define SEG_D 0x040
#define SEG_E 0x080
#define SEG_F 0x100
#define SEG_G 0x200


static const uint16_t seg_7 [] =
{
    SEG_A + SEG_B + SEG_C + SEG_D + SEG_E + SEG_F,          /* 0 */
            SEG_B + SEG_C,                                  /* 1 */
    SEG_A + SEG_B                 + SEG_E + SEG_D + SEG_G,  /* 2 */
    SEG_A + SEG_B + SEG_C + SEG_D                 + SEG_G,  /* 3 */
            SEG_B + SEG_C                 + SEG_F + SEG_G,  /* 4 */
    SEG_A         + SEG_C + SEG_D         + SEG_F + SEG_G,  /* 5 */
    SEG_A         + SEG_C + SEG_D + SEG_E + SEG_F + SEG_G,  /* 6 */
    SEG_A + SEG_B + SEG_C,                                  /* 7 */
    SEG_A + SEG_B + SEG_C + SEG_D + SEG_E + SEG_F + SEG_G,  /* 8 */
    SEG_A + SEG_B + SEG_C + SEG_D + SEG_F + SEG_G,          /* 9 */
    SEG_A + SEG_B + SEG_C + SEG_E + SEG_F + SEG_G,          /* A */
                    SEG_C + SEG_D + SEG_E + SEG_F + SEG_G,  /* b */
    SEG_A                 + SEG_D + SEG_E + SEG_F,          /* C */
            SEG_B + SEG_C + SEG_D + SEG_E +         SEG_G,  /* d */
    SEG_A                 + SEG_D + SEG_E + SEG_F + SEG_G,  /* E */
    SEG_A                         + SEG_E + SEG_F + SEG_G,  /* F */
};
/**
 * Method to initialize the 7-segment display
 */
static void seg7_init()
{
    gpio->leds_ctrl = 0xff;
    gpio->leds_rw = 0;
    gpio->seg7_ctrl = 0x3ff;
    gpio->seg7_rw = 0;
}


/**
 * Method to display a decimal number on the 7-segment
 */
static void seg7_display (int8_t value)
{
    uint16_t dot = 0;

    if (value < 0) {
        value = -value;
        dot = SEG_DOT;
    }

    gpio->seg7_rw = seg_7[value % 10] + 0x1 + dot;
    usleep(8500);

    gpio->seg7_rw = seg_7[value / 10] + 0x2 + dot;
    usleep(8500);
    gpio->seg7_rw = 0x02;
}


int count=0;

void timer_handler()
{
    seg7_display(count);
}

void safesleep(long s, long us) {
    int ret;
    struct timespec req, rem;

    req.tv_sec = s;
    req.tv_nsec = us * 1000;

    while (1) {
        ret = nanosleep(&req, &rem);
        if (ret == -1 && errno == EINTR) {
            req.tv_sec = rem.tv_sec;
            req.tv_nsec = rem.tv_nsec;
        } else if (ret) {
            perror("Nanosleep failed");
        } else {
            return;
        }
    }
}


void sleepfor(long s, long us) {
    struct timeval current, end;
    struct timespec spec;
    int ret;

    CHECKERR(gettimeofday(&end, NULL), "Failed to get time");

    end.tv_sec += s;
    end.tv_usec += us;

    spec.tv_sec = s;
    spec.tv_nsec = us * 1000;

    while (1) {
        ret = nanosleep(&spec, NULL);
        if (ret == -1 && errno != EINTR) {
            perror("Nanosleep failed, restarting");
        }

        CHECKERR(gettimeofday(&current, NULL), "Failed to get time");

        if (current.tv_sec > end.tv_sec) {
            return;
        }

        if (current.tv_sec == end.tv_sec && current.tv_usec >= end.tv_usec) {
            return;
        }

        spec.tv_sec = end.tv_sec - current.tv_usec;
        spec.tv_nsec = (end.tv_usec - current.tv_usec) * 1000;

        if (current.tv_sec < end.tv_sec) {
            spec.tv_sec += 1;

            if (spec.tv_nsec > 1000 * 1000 * 1000) {
                spec.tv_sec += 1;
            } else {
                spec.tv_nsec += 1000 * 1000 * 1000;
            }
        }
    }
}

int main()
{
    int fd;
    int s, ns;

    struct itimerval timer;
    struct sigaction sa;

    fd = open("/dev/mem", O_RDWR);
    CHECKERR(fd, "Could not open /dev/mem");

    gpio = mmap(0, 256, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0xd6000000);
    if (gpio == (void *) 0xFFFFFFFF) {
        CHECKERR(-1, "mmap failed");
    }

    seg7_init();

    // Handler registration
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = timer_handler;
    sigaction(SIGALRM, &sa, NULL);

    // Timer setup
    timer.it_value.tv_sec = timer.it_interval.tv_sec = 0;
    timer.it_value.tv_usec = 1;
    timer.it_interval.tv_usec = 18000;
    setitimer(ITIMER_REAL, &timer, NULL);

    s = SPEED / 10;
    ns = (SPEED % 10) * 100 * 1000;

    while (count < 500) {
        sleepfor(s, ns);
        count = (count + 1);
        printf("Current value is %d\n", count);
    }

    return EXIT_SUCCESS;
}
