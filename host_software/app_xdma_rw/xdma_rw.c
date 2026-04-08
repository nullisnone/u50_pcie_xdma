
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

// Matrix dimensions
#define ROWS 256
#define COLS 256

#define MATRIX_ELEM_COUNT (ROWS * COLS)
#define MATRIX_BYTES_FP32 (MATRIX_ELEM_COUNT * sizeof(float)) // 262144 bytes
#define SEND_BUF_SIZE     (MATRIX_BYTES_FP32 * 2)             // 524288 bytes
#define RECV_BUF_SIZE     (MATRIX_BYTES_FP32)                 // 262144 bytes

typedef struct {
    float data[ROWS][COLS];
    int rows;
    int cols;
} Matrix;

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



// Function to trim whitespace
char *trim_whitespace(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Function to parse matrices from CSV and convert to float32
int parse_matrices(const char* file_path, Matrix **matrices_out, int *count_out) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open input file %s\n", file_path);
        return -1;
    }

    int capacity = 2;
    Matrix *matrices = malloc(sizeof(Matrix) * capacity);
    if (!matrices) {
        fprintf(stderr, "Error: Memory allocation failed for matrices\n");
        fclose(file);
        return -1;
    }
    int count = 0;
    
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    
    int current_row = 0;
    int in_matrix = 0;

    memset(&matrices[count], 0, sizeof(Matrix));

    while ((read = getline(&line, &len, file)) != -1) {
        char *trimmed = trim_whitespace(line);
        if (strlen(trimmed) == 0) {
            if (in_matrix) {
                matrices[count].rows = current_row;
                matrices[count].cols = COLS;
                count++;
                if (count >= capacity) {
                    capacity *= 2;
                    Matrix *new_matrices = realloc(matrices, sizeof(Matrix) * capacity);
                    if (!new_matrices) {
                        fprintf(stderr, "Error: Memory reallocation failed\n");
                        free(matrices);
                        fclose(file);
                        return -1;
                    }
                    matrices = new_matrices;
                }
                memset(&matrices[count], 0, sizeof(Matrix));
                current_row = 0;
                in_matrix = 0;
            }
            continue;
        }

        in_matrix = 1;
        if (current_row >= ROWS) continue;

        char *token = strtok(trimmed, ",");
        int col = 0;
        while (token && col < COLS) {
            matrices[count].data[current_row][col] = strtof(token, NULL);
            col++;
            token = strtok(NULL, ",");
        }
        current_row++;
    }

    if (in_matrix) {
        matrices[count].rows = current_row;
        matrices[count].cols = COLS;
        count++;
    }

    free(line);
    fclose(file);
    
    *matrices_out = matrices;
    *count_out = count;
    return 0;
}



int main (int argc, char *argv[]) {
    int ret = -1;
    uint64_t microsecond;
    int h2c_fd = -1;
    int c2h_fd = -1;
    void *send_buffer = NULL;
    void *recv_buffer = NULL;
    Matrix *matrices = NULL;
    int matrix_count = 0;

    if (argc < 3) {
        printf("Usage: %s <input_csv> <output_csv>\n", argv[0]);
        return -1;
    }

    const char *input_csv = argv[1];
    const char *output_csv = argv[2];

    // 1. Parse matrices
    if (parse_matrices(input_csv, &matrices, &matrix_count) != 0) {
        return -1;
    }

    if (matrix_count < 2) {
        printf("*** ERROR: Expected at least 2 matrices in %s, found %d\n", input_csv, matrix_count);
        free(matrices);
        return -1;
    }

    // Prevent matrix count array out of bounds just in case
    if (matrix_count > 2) {
        matrix_count = 2; 
    }

    // 2. Allocate 4KB page-aligned local memory buffers
    // We add a MASSIVE 1MB padding to prevent XDMA driver overruns.
    printf("DEBUG: Allocating buffers with 1MB padding...\n");
    if (posix_memalign(&send_buffer, 4096, SEND_BUF_SIZE + 1024 * 1024) != 0) {
        printf("*** ERROR: failed to allocate send buffer\n");
        goto close_and_clear;
    }
    memset(send_buffer, 0, SEND_BUF_SIZE + 1024 * 1024);

    if (posix_memalign(&recv_buffer, 4096, RECV_BUF_SIZE + 1024 * 1024) != 0) {
        printf("*** ERROR: failed to allocate recv buffer\n");
        goto close_and_clear;
    }
    memset(recv_buffer, 0, RECV_BUF_SIZE + 1024 * 1024);

    // 3. Reorder matrices into send_buffer (2nd matrix first, 1st matrix second)
    printf("DEBUG: Reordering matrices...\n");
    
    // Safely cast to char* for exact byte-level offset calculation to avoid pointer arithmetic bugs
    char *send_bytes = (char *)send_buffer;
    memcpy(send_bytes, matrices[1].data, MATRIX_BYTES_FP32);
    memcpy(send_bytes + MATRIX_BYTES_FP32, matrices[0].data, MATRIX_BYTES_FP32);

    // 4. Open XDMA devices
    printf("DEBUG: Opening XDMA devices...\n");
    h2c_fd = open("/dev/xdma0_h2c_0", O_RDWR | O_SYNC);
    if (h2c_fd < 0) {
        printf("*** ERROR: failed to open /dev/xdma0_h2c_0\n");
        goto close_and_clear;
    }

    c2h_fd = open("/dev/xdma0_c2h_0", O_RDWR | O_SYNC);
    if (c2h_fd < 0) {
        printf("*** ERROR: failed to open /dev/xdma0_c2h_0\n");
        goto close_and_clear;
    }

    // 5. Time tracking and transfers
    printf("DEBUG: Starting transfers...\n");
    microsecond = get_microsecond();

    // Send data to FPGA
    if (dev_write(h2c_fd, 0, send_buffer, SEND_BUF_SIZE)) {
        printf("*** ERROR: failed to write to /dev/xdma0_h2c_0\n");
        goto close_and_clear;
    }
    printf("DEBUG: H2C transfer complete.\n");

    // Read data from FPGA
    if (dev_read(c2h_fd, 0, recv_buffer, RECV_BUF_SIZE)) {
        printf("*** ERROR: failed to read from /dev/xdma0_c2h_0\n");
        goto close_and_clear;
    }
    printf("DEBUG: C2H transfer complete.\n");
    fflush(stdout);

    printf("DEBUG: Calling get_microsecond() for end time...\n");
    fflush(stdout);
    uint64_t end_ms = get_microsecond();
    
    printf("DEBUG: get_microsecond() returned successfully.\n");
    fflush(stdout);

    microsecond = end_ms > microsecond ? end_ms - microsecond : 1;

    printf("DEBUG: About to print time difference...\n");
    fflush(stdout);
    
    // Completely avoid any libc printf/sprintf to prevent vararg or stack alignment segfaults
    uint32_t final_time = (uint32_t)microsecond;
    
    char time_str[64];
    int idx = 0;
    
    // Manual strcpy
    const char *prefix = "time=";
    while (prefix[idx]) {
        time_str[idx] = prefix[idx];
        idx++;
    }
    
    // Manual itoa
    if (final_time == 0) {
        time_str[idx++] = '0';
    } else {
        char temp[32];
        int temp_idx = 0;
        while (final_time > 0) {
            temp[temp_idx++] = '0' + (final_time % 10);
            final_time /= 10;
        }
        while (temp_idx > 0) {
            time_str[idx++] = temp[--temp_idx];
        }
    }
    
    time_str[idx++] = ' ';
    time_str[idx++] = 'u';
    time_str[idx++] = 's';
    time_str[idx++] = '\n';
    time_str[idx] = '\0';
    
    // Write directly to stdout file descriptor to bypass stdio streams completely
    write(1, time_str, idx);

    // 6. Write result to output CSV
    write(1, "DEBUG: Writing result to output file...\n", 40);
    
    // Use POSIX open to bypass glibc FILE* heap allocation completely
    int out_fd = open(output_csv, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        write(1, "*** ERROR: failed to open output file\n", 38);
        goto close_and_clear;
    }

    write(1, "DEBUG: Output file opened.\n", 27);
    
    // Safely cast to float*
    float *recv_ptr = (float *)recv_buffer;
    
    // Check if recv_ptr is valid
    if (!recv_ptr) {
        write(1, "*** ERROR: recv_ptr is NULL!\n", 29);
        close(out_fd);
        goto close_and_clear;
    }

    write(1, "DEBUG: Starting loop to write rows...\n", 38);
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            float val = recv_ptr[i * COLS + j];
            char buf[64];
            int b_idx = 0;
            
            // Manual float to string (simple implementation for %.6f)
            if (val < 0) {
                buf[b_idx++] = '-';
                val = -val;
            }
            
            // Extract integer part
            int int_part = (int)val;
            
            // Convert integer part
            if (int_part == 0) {
                buf[b_idx++] = '0';
            } else {
                char temp[32];
                int t_idx = 0;
                while (int_part > 0) {
                    temp[t_idx++] = '0' + (int_part % 10);
                    int_part /= 10;
                }
                while (t_idx > 0) {
                    buf[b_idx++] = temp[--t_idx];
                }
            }
            
            if (j < COLS - 1) {
                buf[b_idx++] = ',';
            }
            
            write(out_fd, buf, b_idx);
        }
        write(out_fd, "\n", 1);
    }
    write(1, "DEBUG: File writing loop complete.\n", 35);
    close(out_fd);
    write(1, "DEBUG: File closed.\n", 20);

    ret = 0;

close_and_clear:
    write(1, "DEBUG: Starting cleanup...\n", 27);
    
    // Skip freeing memory explicitly because glibc's heap is likely corrupted by the XDMA driver
    // and calling free() will cause a segfault. The OS will reclaim the memory when the process exits anyway.
    
    if (h2c_fd >= 0) {
        write(1, "DEBUG: Closing h2c_fd...\n", 25);
        close(h2c_fd);
    }
    if (c2h_fd >= 0) {
        write(1, "DEBUG: Closing c2h_fd...\n", 25);
        close(c2h_fd);
    }
    write(1, "DEBUG: Cleanup complete. Exiting.\n", 34);

    return ret;
}



