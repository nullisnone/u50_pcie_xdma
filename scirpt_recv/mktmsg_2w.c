#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>

#include <signal.h>

#include "/usr/local/include/ndpp/ndpp.h"

// Global pointers for signal handler cleanup
ndpp_dev_t* g_ndpp_dev = NULL;
ndpp_rx_t* g_rx_chn = NULL;
char* g_dev_name = NULL;
uint8_t* g_send_buf_1 = NULL;
uint8_t* g_send_buf_0 = NULL;
uint8_t* g_recv_buf = NULL;

// Signal handler for Ctrl-C
void sigint_handler(int sig) {
    printf("\nCaught signal %d (Ctrl-C). Cleaning up resources...\n", sig);
    
    if (g_rx_chn) ndpp_rx_destroy(g_rx_chn);
    if (g_ndpp_dev) ndpp_dev_destroy(g_ndpp_dev);
    if (g_dev_name) free(g_dev_name);
    if (g_send_buf_1) free(g_send_buf_1);
    if (g_send_buf_0) free(g_send_buf_0);
    if (g_recv_buf) free(g_recv_buf);
    
    exit(1);
}

#define CFG_FILE "mktmsg_lb_cfg.json"

// Matrix dimensions
#define ROWS 256
#define COLS 256

#define MATRIX_BUF_SIZE (ROWS * COLS)   // 65536 bytes (1 matrix, uint8)
#define RECV_BUF_SIZE (ROWS * COLS * 4) // 262144 bytes (1 matrix, uint32)

// Structure to hold a matrix
typedef struct {
    int data[ROWS][COLS];
    int rows;
    int cols;
} Matrix;

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

// Function to parse device name from JSON config
char* get_dev_name(const char* config_file) {
    FILE *file = fopen(config_file, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open config file %s\n", config_file);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *buffer = malloc(length + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    // Simple manual JSON parsing for "dev_name"
    char *key = "\"dev_name\"";
    char *pos = strstr(buffer, key);
    if (!pos) {
        fprintf(stderr, "Error: 'dev_name' not found in config file.\n");
        free(buffer);
        return NULL;
    }

    char *colon = strchr(pos, ':');
    if (!colon) { free(buffer); return NULL; }

    char *start_quote = strchr(colon, '\"');
    if (!start_quote) { free(buffer); return NULL; }

    char *end_quote = strchr(start_quote + 1, '\"');
    if (!end_quote) { free(buffer); return NULL; }

    int name_len = end_quote - start_quote - 1;
    char *dev_name = malloc(name_len + 1);
    strncpy(dev_name, start_quote + 1, name_len);
    dev_name[name_len] = '\0';

    free(buffer);
    return dev_name;
}

// Function to parse matrices from CSV
int parse_matrices(const char* file_path, Matrix **matrices_out, int *count_out) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open input file %s\n", file_path);
        return -1;
    }

    // Allocate initial space for 2 matrices
    int capacity = 2;
    Matrix *matrices = malloc(sizeof(Matrix) * capacity);
    int count = 0;
    
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    
    int current_row = 0;
    int in_matrix = 0;

    // Initialize first matrix
    memset(&matrices[count], 0, sizeof(Matrix));

    while ((read = getline(&line, &len, file)) != -1) {
        char *trimmed = trim_whitespace(line);
        if (strlen(trimmed) == 0) {
            if (in_matrix) {
                // End of current matrix
                matrices[count].rows = current_row;
                matrices[count].cols = COLS; // Assume full cols for now
                count++;
                if (count >= capacity) {
                    capacity *= 2;
                    matrices = realloc(matrices, sizeof(Matrix) * capacity);
                }
                memset(&matrices[count], 0, sizeof(Matrix));
                current_row = 0;
                in_matrix = 0;
            }
            continue;
        }

        in_matrix = 1;
        if (current_row >= ROWS) continue; // Skip extra rows if any

        char *token = strtok(trimmed, ",");
        int col = 0;
        while (token && col < COLS) {
            matrices[count].data[current_row][col] = atoi(token);
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

// Helper to get monotonic time in microseconds
long long get_time_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

int main(int argc, char* argv[]) {
    // Register signal handler for Ctrl-C
    signal(SIGINT, sigint_handler);

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    char *input_file = argv[1];
    char *output_file = argv[2];

    // Load Config
    char *dev_name = get_dev_name(CFG_FILE);
    g_dev_name = dev_name;
    if (!dev_name) return 1;
    printf("Device Name: %s\n", dev_name);

    // Load Matrices
    Matrix *matrices;
    int matrix_count = 0;
    if (parse_matrices(input_file, &matrices, &matrix_count) != 0) {
        free(dev_name);
        return 1;
    }

    if (matrix_count != 2) {
        fprintf(stderr, "Error: Expected 2 matrices, found %d\n", matrix_count);
        free(matrices);
        free(dev_name);
        return 1;
    }

    // Initialize NDPP
    ndpp_dev_t* ndpp_dev = ndpp_dev_create(dev_name);
    g_ndpp_dev = ndpp_dev;
    if (!ndpp_dev) {
        fprintf(stderr, "Failed to initialize NDPP device.\n");
        free(matrices);
        free(dev_name);
        return 1;
    }

    // Soft Reset
    /*
    if (ndpp_register_write32(ndpp_dev, 0x0000, 0x01) < 0) {
        fprintf(stderr, "Failed to reset device.\n");
        ndpp_dev_destroy(ndpp_dev);
        free(matrices);
        free(dev_name);
        return 1;
    }
    printf("yusur_ndpp_reset successful.\n");
    */

    // Create Channels
    int dma_channel = 0;
    // TX channel is not needed for direct memory write, but we keep RX for receiving
    ndpp_rx_t* rx_chn = ndpp_rx_create(ndpp_dev, dma_channel);
    g_rx_chn = rx_chn;

    if (!rx_chn) {
        fprintf(stderr, "Failed to create RX channel: %s\n", ndpp_strerror(errno));
        ndpp_dev_destroy(ndpp_dev);
        free(matrices);
        free(dev_name);
        return 1;
    }


    uint8_t *send_buf_1 = malloc(MATRIX_BUF_SIZE);
    g_send_buf_1 = send_buf_1;
    uint8_t *send_buf_0 = malloc(MATRIX_BUF_SIZE);
    g_send_buf_0 = send_buf_0;

    if (!send_buf_1 || !send_buf_0) {
        fprintf(stderr, "Failed to allocate send buffers.\n");
        goto cleanup;
    }

    // Fill buffer with Matrix 1 data
    int buf_idx_1 = 0;
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            int val = (r < matrices[1].rows) ? matrices[1].data[r][c] : 0;
            send_buf_1[buf_idx_1++] = (uint8_t)val;
        }
    }
    // Fill buffer with Matrix 0 data
    int buf_idx_0 = 0;
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            int val = (r < matrices[0].rows) ? matrices[0].data[r][c] : 0;
            send_buf_0[buf_idx_0++] = (uint8_t)val;
        }
    }

    uint8_t *recv_buf = malloc(RECV_BUF_SIZE);
    g_recv_buf = recv_buf;
    if (!recv_buf) {
        fprintf(stderr, "Failed to allocate receive buffer\n");
        goto cleanup;
    }
    
    // Prepare Buffer and Send via Direct Memory Write
    long long start_time = get_time_us();
    int first_byte_received = 0;

    // Direct Memory Write
    const uint64_t addr_matrix_1 = 0x000000;
    int ret_write_1 = ndpp_direct_memory_write(ndpp_dev, addr_matrix_1, send_buf_1, MATRIX_BUF_SIZE);
    
    if (ret_write_1 < 0) {
        fprintf(stderr, "ndpp_direct_memory_write for matrix 1 failed: %s\n", ndpp_strerror(errno));
        goto cleanup;
    }
    printf("Matrix 1 write to 0x%lx successful. Sent %d bytes.\n", addr_matrix_1, MATRIX_BUF_SIZE);

    const uint64_t addr_matrix_0 = 0x010000;
    int ret_write_0 = ndpp_direct_memory_write(ndpp_dev, addr_matrix_0, send_buf_0, MATRIX_BUF_SIZE);
    
    if (ret_write_0 < 0) {
        fprintf(stderr, "ndpp_direct_memory_write for matrix 0 failed: %s\n", ndpp_strerror(errno));
        goto cleanup;
    }
    printf("Matrix 0 write to 0x%lx successful. Sent %d bytes.\n", addr_matrix_0, MATRIX_BUF_SIZE);

    free(send_buf_1);
    g_send_buf_1 = NULL;
    free(send_buf_0);
    g_send_buf_0 = NULL;

/*
    // Prepare Send Buffer (32 bytes of 0xFF)
    uint8_t send_buf_fin[32];
    memset(send_buf_fin, 0xff, 32);

    // Direct Memory Write
    const uint64_t target_addr = 0x020000;
    printf("Sending 32 bytes of 0xFF to address 0x%lx...\n", target_addr);
    
    // Optional: Check memory availability
    void *mem_base = NULL;
    size_t mem_size = 0;
    if (ndpp_direct_memory_get(ndpp_dev, &mem_base, &mem_size) == 0) {
        printf("Direct Memory Info: Base=%p, Size=%zu bytes\n", mem_base, mem_size);
    }

    int ret_write_fin = ndpp_direct_memory_write(ndpp_dev, target_addr, send_buf_fin, 32);
    if (ret_write_fin < 0) {
        fprintf(stderr, "send_buf_fin ndpp_direct_memory_write failed: %s\n", ndpp_strerror(errno));
        goto cleanup;
    }
    printf("send_buf_fin ndpp_direct_memory_write successful.\n");
*/

    // Receive Logic
    size_t total_received = 0;
    long long recv_start_time = get_time_us();
    printf("Start receiving data...\n");

    while (total_received < RECV_BUF_SIZE) {
        ssize_t ret = ndpp_receive_0(rx_chn, recv_buf + total_received, RECV_BUF_SIZE - total_received, 0);
        printf("Received %zd bytes\n", ret);
        if (ret > 0) {
            if (!first_byte_received) {
                long long now = get_time_us();
                long long latency_us = now - start_time;
                printf("Latency (First Tx -> First Rx): %lld us (%.3f ms)\n", latency_us, latency_us / 1000.0);
                first_byte_received = 1;
            }
            total_received += ret;
        }

        // Timeout check (10 seconds)
        if ((get_time_us() - recv_start_time) > 10000000) {
            fprintf(stderr, "Timeout waiting for data.\n");
            break;
        }

        if (ret <= 0) {
            if (ret < 0 && errno != NDPP_EAGAIN) {
                fprintf(stderr, "Receive error: %s\n", ndpp_strerror(errno));
            }
        }
    }

    if (total_received >= RECV_BUF_SIZE) {
        printf("Received full batch (%zu bytes).\n", total_received);

        FILE *out_file = fopen(output_file, "w");
        if (!out_file) {
            fprintf(stderr, "Error: Could not open output file %s\n", output_file);
        } else {
            // Parse Payload (no header)
            const uint32_t *data_ptr = (const uint32_t *)recv_buf;

            for (int r = 0; r < ROWS; ++r) {
                for (int c = 0; c < COLS; ++c) {
                    uint32_t val = data_ptr[r * COLS + c];
                    fprintf(out_file, "%u", val);
                    if (c < COLS - 1) fprintf(out_file, ",");
                }
                fprintf(out_file, "\n");
            }
            fclose(out_file);
        }
    }
    
    free(recv_buf);
    g_recv_buf = NULL;

cleanup:
    // tx_chn is not created
    if (rx_chn) ndpp_rx_destroy(rx_chn);
    if (ndpp_dev) ndpp_dev_destroy(ndpp_dev);
    free(matrices);
    free(dev_name);

    return 0;
}
