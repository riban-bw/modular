#include <stdio.h> // Provides printf
#include <stdlib.h> // Provides exit
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

int file_i2c;
int length;
unsigned char buffer[60] = {0};

void main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: %s i2c-addr\n", argv[0]);
        exit(-1);
    }

    //----- OPEN THE I2C BUS -----
    char *filename = (char *)"/dev/i2c-1";
    if ((file_i2c = open(filename, O_RDWR)) < 0)
    {
        printf("Failed to open the i2c bus");
        return;
    }

    int addr = 100;
    if (ioctl(file_i2c, I2C_SLAVE, addr) < 0)
    {
        printf("Failed to acquire bus access and/or talk to slave.\n");
        return;
    }

    //----- WRITE BYTES -----
    buffer[0] = 0x00;
    length = 1;
    if (write(file_i2c, buffer, length) != length)
    {
        printf("Failed to write to the i2c bus.\n");
    }

    //----- READ BYTES -----
    length = 9;
    if (read(file_i2c, buffer, length) != length) // read() returns the number of bytes actually read, if it doesn't match then an error occurred (e.g. no response from the device)
    {
        // ERROR HANDLING: i2c transaction failed
        printf("Failed to read from the i2c bus.\n");
    }
    else
    {
        for (int i = 0; i < length; ++i)
            printf("0x%02x \n", buffer[i]);
        printf("\n");
    }

}