#define _GNU_SOURCE 1
#define _POSIX_SOURCE 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

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

int val1 = SEG_A + SEG_B + SEG_C + SEG_D + SEG_E + SEG_F + 0x1,
    val2 = SEG_A + SEG_B + SEG_C + SEG_D + SEG_E + SEG_F + 0x2;

void * worker_func()
{
    gpio->leds_ctrl = 0xff;
    gpio->leds_rw = 0;
    gpio->seg7_ctrl = 0x3ff;

    while (1) {
        gpio->seg7_rw = val1;
        usleep(8500);
        gpio->seg7_rw = val2;
        usleep(8500);
    }
}

int main()
{
    int fd, count=0;
    pthread_t worker;

    fd = open("/dev/mem", O_RDWR);
    CHECKERR(fd, "Could not open /dev/mem");

    gpio = mmap(0, 256, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0xd6000000);
    if (gpio == (void *) 0xFFFFFFFF) {
        CHECKERR(-1, "mmap failed");
    }

    pthread_create(&worker, NULL, worker_func, NULL);

    while (count++ < 100) {
        usleep(SPEED * 100 * 1000);
        val1 = seg_7[count % 10] + 0x1;
        val2 = seg_7[count / 10] + 0x2;
    }

    return EXIT_SUCCESS;
}
