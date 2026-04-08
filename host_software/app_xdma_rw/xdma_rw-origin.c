
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <time.h>

// function : dev_read
// description : read data from device to local memory (buffer), (i.e. device-to-host)
// parameter :
//       dev_fd : device instance
//       addr   : source address in the device
//       buffer : buffer base pointer
//       size   : data size
// return:
//       int : 0=success,  -1=failed
int dev_read (int dev_fd, uint64_t addr, void *buffer, uint64_t size) {
    if ( (ssize_t)size != pread(dev_fd, buffer, size, addr) )                    // read device to buffer atomically at offset
        return -1;                                                               // read failed
    return 0;
}


// function : dev_write
// description : write data from local memory (buffer) to device, (i.e. host-to-device)
// parameter :
//       dev_fd : device instance
//       addr   : target address in the device
//       buffer : buffer base pointer
//       size   : data size
// return:
//       int : 0=success,  -1=failed
int dev_write (int dev_fd, uint64_t addr, void *buffer, uint64_t size) {
    if ( (ssize_t)size != pwrite(dev_fd, buffer, size, addr) )                   // write device from buffer atomically at offset
        return -1;                                                               // write failed
    return 0;
}




// function : get_microsecond
// description : get time in microsecond
static uint64_t get_microsecond () {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec) * 1000000UL + (uint64_t)(ts.tv_nsec) / 1000UL;
}



// function : parse_uint
// description : get a uint64 value from string (support hexadecimal and decimal)
int parse_uint (char *string, uint64_t *pvalue) {
    if ( string[0] == '0'  &&  string[1] == 'x' )                // HEX format "0xXXXXXXXX"
        return sscanf( &(string[2]), "%lx", pvalue);
    else                                                         // DEC format
        return sscanf(   string    , "%lu", pvalue);
}




#define  DMA_MAX_SIZE   0x10000000UL



char USAGE [] = 
    "Usage: \n"
    "\n"
    "  write device (host-to-device):\n"
    "    %s <file_name> to <device_name> <address_in_the_device> <size>\n"
    "  example:\n"
    "    %s data.bin to /dev/xdma0_h2c_0 0x100000 0x10000\n"
    "\n"
    "  read device (device-to-host):\n"
    "    %s <file_name> from <device_name> <address_in_the_device> <size>\n"
    "  example:\n"
    "    %s data.bin from /dev/xdma0_c2h_0 0x100000 0x10000\n" ;



int main (int argc, char *argv[]) {
    int   ret = -1;
    
    uint64_t microsecond;
    char usage_string [1024];
    
    char *dev_name, *file_name;
    char direction;
    uint64_t address, size;
    
    int   dev_fd = -1;
    int   out_fd = -1;
    void *buffer = NULL;
    
    sprintf(usage_string, USAGE, argv[0], argv[0], argv[0], argv[0] );
    
    if (argc < 6) {                                                // not enough argument
        puts(usage_string);
        return -1;
    }
    
    
    file_name = argv[1];
    direction = argv[2][0];
    dev_name  = argv[3];
    
    if ( 0 == parse_uint(argv[4], &address) ) {                    // parse the address in the device
        puts(usage_string);
        return -1;
    }
    
    if ( 0 == parse_uint(argv[5], &size ) ) {                      // parse the size in the device
        puts(usage_string);
        return -1;
    }
    
    // print information:
    if        (direction == 't') {                                 // to (write device, host-to-device)
        printf("from : %s\n" , file_name);
        printf("to   : %s   addr=0x%lx\n" , dev_name, address);
        printf("size : 0x%lx\n\n" , size);
    } else if (direction == 'f') {                                 // from (read device, device-to-host)
        printf("from : %s   addr=0x%lx\n" , dev_name, address);
        printf("to   : %s\n" , file_name);
        printf("size : 0x%lx\n\n" , size);
    } else {
        puts(usage_string);
        return -1;
    }
    
    
    if (size > DMA_MAX_SIZE  ||  size == 0) {
        printf("*** ERROR: DMA size must larger than 0 and NOT be larger than %lu\n", DMA_MAX_SIZE);
        return -1;
    }
    
    printf("DEBUG: Opening device...\n");
    fflush(stdout);
    dev_fd = open(dev_name, O_RDWR | O_SYNC);                                // open device with synchronous I/O
    if (dev_fd < 0) {
        printf("*** ERROR: failed to open device %s\n", dev_name);
        goto close_and_clear;
    }
    
    printf("DEBUG: Opening file...\n");
    fflush(stdout);
    if (direction == 't') {
        out_fd = open(file_name, O_RDONLY);
    } else {
        out_fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    }
    if (out_fd < 0) {
        printf("*** ERROR: failed to open file %s\n", file_name);
        goto close_and_clear;
    }
    
    // allocate 4KB page-aligned local memory (buffer) for safe DMA transfers
    // We add a MASSIVE 1MB padding. If the FPGA AXI Stream sends a packet larger than 'size',
    // the XDMA driver might overrun the buffer. The 1MB padding absorbs it.
    // Also, because we called fopen() FIRST, file_p is safely behind the buffer in memory!
    printf("DEBUG: Calling posix_memalign...\n");
    fflush(stdout);
    if (posix_memalign(&buffer, 4096, size + 1024 * 1024) != 0) {
        printf("*** ERROR: failed to allocate buffer\n");
        buffer = NULL;
        goto close_and_clear;
    }
    
    printf("DEBUG: Memset buffer...\n");
    fflush(stdout);
    // Force OS to map the pages
    memset(buffer, 0, size + 1024 * 1024);
    
    microsecond = get_microsecond();                                         // start time
    
    if (direction == 't') {
        printf("DEBUG: Reading from file %s...\n", file_name);
        if ( size != read(out_fd, buffer, size) ) {                          // local file -> local buffer
            printf("*** ERROR: failed to read %s\n", file_name);
            goto close_and_clear;
        }
        printf("DEBUG: Writing to device %s...\n", dev_name);
        if ( dev_write(dev_fd, address, buffer, size) ) {                    // local buffer -> device
            printf("*** ERROR: failed to write %s\n", dev_name);
            goto close_and_clear;
        }
        printf("DEBUG: Device write completed.\n");
    } else {
        printf("DEBUG: Starting dev_read... (addr=0x%lx, buffer=%p, size=%lu)\n", address, buffer, size);
        fflush(stdout);
        
        int read_ret = dev_read(dev_fd, address, buffer, size);
        printf("DEBUG: dev_read returned %d. Starting write... (fd=%d)\n", read_ret, out_fd);
        fflush(stdout);
        
        if ( read_ret ) {                     // device -> local buffer
            printf("*** ERROR: failed to read %s\n", dev_name);
            goto close_and_clear;
        }
        
        // Use raw POSIX write instead of fwrite
        ssize_t write_ret = write(out_fd, buffer, size);
        printf("DEBUG: write returned %zd. File write completed.\n", write_ret);
        fflush(stdout);
        
        if ( size != (size_t)write_ret ) {                     // local buffer -> local file
            printf("*** ERROR: failed to write %s\n", file_name);
            goto close_and_clear;
        }
    }
    
    microsecond = get_microsecond() - microsecond;                           // get time consumption
    microsecond = (microsecond > 0) ? microsecond : 1;                       // avoid divide-by-zero
    
    printf("DEBUG: Time calculation completed. Microsecond: %llu\n", (unsigned long long)microsecond);
    
    printf("time=%llu us\n", (unsigned long long)microsecond);
    
    ret = 0;
    
close_and_clear:
    
    if (buffer != NULL) {
        free(buffer);
    }
    
    if (dev_fd >= 0) {
        close(dev_fd);
    }
    
    if (out_fd >= 0) {
        close(out_fd);
    }
    
    return ret;
}



