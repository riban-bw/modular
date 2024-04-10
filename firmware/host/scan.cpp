#include <bcm2835.h>
#include <cstdio>
#include <unistd.h>
#include <time.h>

#define PIN 27
#define DELAY 1

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr,  "Syntax: %s i2c_addr\n", argv[0]);
        return -1;
    }
    if (!bcm2835_init()) {
      fprintf(stderr, "Failed to initalise bcm2835\n");
      return 1;
    }

    uint8_t i2cAddress = atoi(argv[1]);
    if (i2cAddress < 10 || i2cAddress > 0x77) {
        fprintf(stderr, "I2C address out of range 10..119\n");
        return -1;
    }

    while (true) {
        // Configure pin to pull high when listenting and low when sending clock
        bcm2835_gpio_set_pud(PIN, BCM2835_GPIO_PUD_UP);
        bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_INPT);
        bcm2835_gpio_write(PIN, LOW);
        // Send start bit (>200us)
        usleep(300);

        // Clock presence bit
        bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_OUTP);
        usleep(1); // Actually about 60uS
        bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_INPT);
        usleep(1); // Actually about 60uS
        if (bcm2835_gpio_lev(PIN)) {
            fprintf(stderr, "No more devices detected\n");
            bcm2835_close();
            return 0;
        }
        usleep(30);

        uint8_t uid[] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
        uint8_t checksum = 0;

        for (uint8_t i = 0; i < 13; ++i) {
            for (uint8_t j = 0; j < 8; ++j) {
                bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_OUTP);
                usleep(1); // Actually about 60uS
                bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_INPT);
                usleep(1); // Actually about 60uS
                if (!bcm2835_gpio_lev(PIN))
                    uid[i] |= 1 << j;
                usleep(30);
            }
            checksum += uid[i];
        }


        // Send I2C address (0x00 if checksum error)
        for (uint8_t i = 0; i < 8; ++i) {
            bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_OUTP);
            usleep(1); // Actually about 60uS
            if (!checksum && ((i2cAddress >> i) & 1))
                // Valid and bit set so stetch clock
                usleep(60);
            bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_INPT);
            usleep(1);
        }

        if (checksum)
            fprintf(stderr, "Checksum error\n");
        else {
            for (uint8_t i = 0; i < 13; ++i)
                printf("%02x", uid[i]);
            printf("\n");
            break;
        }
    }
    
    bcm2835_close();
    return 0;
}
