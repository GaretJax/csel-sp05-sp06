#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/errno.h> 


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
#define SEG_A   0x008
#define SEG_B   0x010
#define SEG_C   0x020
#define SEG_D   0x040
#define SEG_E   0x080
#define SEG_F   0x100
#define SEG_G   0x200

static const uint16_t seg_7 [] =
{
    SEG_A + SEG_B + SEG_C + SEG_D + SEG_E + SEG_F           ,   /* 0 */
            SEG_B + SEG_C                                   ,   /* 1 */
    SEG_A + SEG_B +         SEG_D + SEG_E +         SEG_G   ,   /* 2 */
    SEG_A + SEG_B + SEG_C + SEG_D +                 SEG_G   ,   /* 3 */
            SEG_B + SEG_C +                 SEG_F + SEG_G   ,   /* 4 */
    SEG_A +         SEG_C + SEG_D +         SEG_F + SEG_G   ,   /* 5 */
    SEG_A +         SEG_C + SEG_D + SEG_E + SEG_F + SEG_G   ,   /* 6 */
    SEG_A + SEG_B + SEG_C                                   ,   /* 7 */
    SEG_A + SEG_B + SEG_C + SEG_D + SEG_E + SEG_F + SEG_G   ,   /* 8 */
    SEG_A + SEG_B + SEG_C + SEG_D +         SEG_F + SEG_G   ,   /* 9 */
    SEG_A + SEG_B + SEG_C +         SEG_E + SEG_F + SEG_G   ,   /* A */
                    SEG_C + SEG_D + SEG_E + SEG_F + SEG_G   ,   /* B */
    SEG_A +                 SEG_D + SEG_E + SEG_F           ,   /* C */
            SEG_B + SEG_C + SEG_D + SEG_E +         SEG_G   ,   /* D */
    SEG_A +                 SEG_D + SEG_E + SEG_F + SEG_G   ,   /* E */
    SEG_A +                         SEG_E + SEG_F + SEG_G   ,   /* F */
};


/**
 * Method to initialize the 7-segment display
 */
static void seg7_init()
{
    static bool is_initialized = false;

    if (!is_initialized) {
        gpio->leds_ctrl = 0xff;
        gpio->leds_rw = 0;
        gpio->seg7_ctrl = 0x3ff;
        gpio->seg7_rw = 0;
        is_initialized = true;
    }
}


/**
 * Method to display a decimal number on the 7-segment
 */
static void seg7_display (int8_t value)
{
    int i;
    uint16_t dot = 0;

    seg7_init();

    if (value < 0) {
        value = -value;
        dot = SEG_DOT;
    }

    for (i=0; i<10; i++) {
        gpio->seg7_rw = seg_7[value % 10] + 0x1 + dot;
    }
    gpio->seg7_rw = 0x1;

    for (i=0; i<10; i++) {
        gpio->seg7_rw = seg_7[value / 10] + 0x2 + dot;
    }
    gpio->seg7_rw = 0x2;
}


int main(void)
{
    int fd, i, j;

    fd=open("/dev/mem", O_RDWR);

    if (fd<0) {
        printf("Could not open /dev/mem: error=%i\n", fd);
        return fd;
    }

    gpio = mmap(0, 256, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0xd6000000);

    if (gpio == (void *) 0xFFFFFFFF) {
        printf("mmap failed, error: %i:%s \n",errno,strerror(errno));
        return(-1);
    }

    for (i=0; i<100; i++) {
        for (j=0; j<100; j++){
            seg7_display(i);
        }
    }

    return 0;
}

