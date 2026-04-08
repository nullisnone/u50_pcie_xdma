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
uint8_t* g_recv_buf = NULL;

void sigint_handler(int sig) {
    printf("\nCaught signal %d (Ctrl-C). Cleaning up resources...\n", sig);
    if (g_rx_chn) ndpp_rx_destroy(g_rx_chn);
    if (g_ndpp_dev) ndpp_dev_destroy(g_ndpp_dev);
    if (g_dev_name) free(g_dev_name);
    if (g_recv_buf) free(g_recv_buf);
    exit(1);
}

#define CFG_FILE "mktmsg_lb_cfg.json"

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
    if (!buffer) { fclose(file); return NULL; }
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    // Simple manual JSON parsing for "dev_name"
    char *key = "\"dev_name\"";
    char *pos = strstr(buffer, key);
    if (!pos) { free(buffer); return NULL; }
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

int main(int argc, char* argv[]) {
    signal(SIGINT, sigint_handler);

    // Load Config
    char *dev_name = get_dev_name(CFG_FILE);
    g_dev_name = dev_name;
    if (!dev_name) {
        fprintf(stderr, "Error: Failed to load device name from %s\n", CFG_FILE);
        return 1;
    }
    printf("Device Name: %s\n", dev_name);

    // Initialize NDPP
    ndpp_dev_t* ndpp_dev = ndpp_dev_create(dev_name);
    g_ndpp_dev = ndpp_dev;
    if (!ndpp_dev) {
        fprintf(stderr, "Failed to initialize NDPP device.\n");
        free(dev_name);
        return 1;
    }

    // Soft Reset
    if (ndpp_register_write32(ndpp_dev, 0x0000, 0x01) < 0) {
        fprintf(stderr, "Failed to reset device.\n");
        ndpp_dev_destroy(ndpp_dev);
        free(dev_name);
        return 1;
    }
    printf("yusur_ndpp_reset successful.\n");

    // Create RX Channel
    int dma_channel = 0;
    ndpp_rx_t* rx_chn = ndpp_rx_create(ndpp_dev, dma_channel);
    g_rx_chn = rx_chn;
    if (!rx_chn) {
        fprintf(stderr, "Failed to create RX channel: %s\n", ndpp_strerror(errno));
        ndpp_dev_destroy(ndpp_dev);
        free(dev_name);
        return 1;
    }

    // Prepare Send Buffer (32 bytes of 0xFF)
    uint8_t send_buf[32];
    memset(send_buf, 0xff, 32);

    // Direct Memory Write
    const uint64_t target_addr = 0x020000;
    printf("Sending 32 bytes of 0xFF to address 0x%lx...\n", target_addr);
    
    // Optional: Check memory availability
    void *mem_base = NULL;
    size_t mem_size = 0;
    if (ndpp_direct_memory_get(ndpp_dev, &mem_base, &mem_size) == 0) {
        printf("Direct Memory Info: Base=%p, Size=%zu bytes\n", mem_base, mem_size);
    }

    int ret_write = ndpp_direct_memory_write(ndpp_dev, target_addr, send_buf_fin, 32);
    if (ret_write < 0) {
        fprintf(stderr, "ndpp_direct_memory_write failed: %s\n", ndpp_strerror(errno));
        goto cleanup;
    }
    printf("send_buf_fin ndpp_direct_memory_write successful.\n");

    // Receive Logic
    #define RECV_BUF_SIZE 256*256*4
    uint8_t *recv_buf = malloc(RECV_BUF_SIZE);
    g_recv_buf = recv_buf;
    if (!recv_buf) {
        fprintf(stderr, "Failed to allocate receive buffer\n");
        goto cleanup;
    }

    printf("Start receiving data (Ctrl-C to stop)...\n");

    while (1) {
        // Non-blocking receive attempt using NDPP_NONBLOCK flag if supported, 
        // or just rely on the API blocking behavior. 
        // Assuming 0 flag is blocking. We loop forever.
        ssize_t ret = ndpp_receive_0(rx_chn, recv_buf, RECV_BUF_SIZE, 0);
        
        if (ret > 0) {
            printf("Received %zd bytes: ", ret);
            // Print first 32 bytes in Hex
            for(int i=0; i<ret && i<32; i++) {
                 printf("%02X ", recv_buf[i]);
            }
            if(ret > 32) printf("...");
            printf("\n");
        } else if (ret < 0) {
             // NDPP_EAGAIN means no data available in non-blocking mode, 
             // but with flag 0 it might block until data arrives.
             if (errno != NDPP_EAGAIN) {
                 fprintf(stderr, "Receive error: %s\n", ndpp_strerror(errno));
                 break;
             }
        }
        else printf("No data received, waiting...\n");
    }

cleanup:
    if (rx_chn) ndpp_rx_destroy(rx_chn);
    if (ndpp_dev) ndpp_dev_destroy(ndpp_dev);
    free(dev_name);
    if (recv_buf) free(recv_buf);

    return 0;
}