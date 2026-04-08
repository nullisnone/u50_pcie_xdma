#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <thread>
#include <chrono>
#include <iomanip>
#include <algorithm>

// Include NDPP header
// Ensure this path is correct in your environment
extern "C" {
#include "/usr/local/include/ndpp_cpython/ndpp_define.h"
}

// Configuration file
const std::string CFG_FILE = "mktmsg_lb_cfg.json";

// Matrix dimensions
const int ROWS = 256;
const int COLS = 256;
const int CHUNK_ROWS = 16;
const int PACKET_PAYLOAD_SIZE = CHUNK_ROWS * COLS; // 16 * 256 = 4096
const int HEADER_SIZE = 8;
const int PACKET_SIZE = HEADER_SIZE + PACKET_PAYLOAD_SIZE; // 4104
const int TOTAL_PACKETS_PER_MATRIX = ROWS / CHUNK_ROWS; // 16

const int RECV_HEADER_SIZE = 8;
const int RECV_PAYLOAD_SIZE = ROWS * COLS * 4; // 256 * 256 * 4 = 262144
const int RECV_TOTAL_SIZE = RECV_HEADER_SIZE + RECV_PAYLOAD_SIZE; // 262152

// Function to parse device name from JSON config
std::string get_dev_name(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open config file " << config_file << std::endl;
        return "";
    }

    std::string line;
    std::string content;
    while (std::getline(file, line)) {
        content += line;
    }

    // Simple JSON parsing to find "dev_name"
    size_t key_pos = content.find("\"dev_name\"");
    if (key_pos == std::string::npos) {
        std::cerr << "Error: 'dev_name' not found in config file." << std::endl;
        return "";
    }

    size_t colon_pos = content.find(":", key_pos);
    if (colon_pos == std::string::npos) return "";

    size_t start_quote = content.find("\"", colon_pos);
    if (start_quote == std::string::npos) return "";

    size_t end_quote = content.find("\"", start_quote + 1);
    if (end_quote == std::string::npos) return "";

    return content.substr(start_quote + 1, end_quote - start_quote - 1);
}

// Function to parse matrices from CSV
std::vector<std::vector<std::vector<int>>> parse_matrices(const std::string& file_path) {
    std::vector<std::vector<std::vector<int>>> matrices;
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open input file " << file_path << std::endl;
        return matrices;
    }

    std::string line;
    std::vector<std::vector<int>> current_matrix;

    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty()) {
            if (!current_matrix.empty()) {
                matrices.push_back(current_matrix);
                current_matrix.clear();
            }
            continue;
        }

        std::vector<int> row;
        std::stringstream ss(line);
        std::string token;
        while (std::getline(ss, token, ',')) {
            // Trim token
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            if (token.empty()) continue;

            try {
                row.push_back(std::stoi(token));
            } catch (...) {
                // Ignore invalid values
            }
        }
        if (!row.empty()) {
            current_matrix.push_back(row);
        }
    }
    if (!current_matrix.empty()) {
        matrices.push_back(current_matrix);
    }
    return matrices;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_file>" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    // Load Config
    std::string dev_name = get_dev_name(CFG_FILE);
    if (dev_name.empty()) {
        return 1;
    }
    std::cout << "Device Name: " << dev_name << std::endl;

    // Load Matrices
    auto matrices = parse_matrices(input_file);
    if (matrices.size() != 2) {
        std::cerr << "Error: Expected 2 matrices, found " << matrices.size() << std::endl;
        return 1;
    }

    // Check dimensions
    for (size_t i = 0; i < matrices.size(); ++i) {
        if (matrices[i].size() > ROWS) {
            std::cerr << "Error: Matrix " << i + 1 << " rows > " << ROWS << std::endl;
            return 1;
        }
        if (!matrices[i].empty() && matrices[i][0].size() > COLS) {
            std::cerr << "Error: Matrix " << i + 1 << " cols > " << COLS << std::endl;
            return 1;
        }
    }

    // Initialize NDPP
    ndpp_dev_t* ndpp_dev = ndpp_dev_create(dev_name.c_str());
    if (!ndpp_dev) {
        std::cerr << "Failed to initialize NDPP device." << std::endl;
        return 1;
    }

    // Soft Reset
    if (ndpp_register_write32(ndpp_dev, 0x0000, 0x01) < 0) {
        std::cerr << "Failed to reset device." << std::endl;
        ndpp_dev_destroy(ndpp_dev);
        return 1;
    }
    std::cout << "yusur_ndpp_reset successful." << std::endl;
    //std::this_thread::sleep_for(std::chrono::seconds(1));

    // Create Channels
    int dma_channel = 0;
    ndpp_tx_t* tx_chn = ndpp_tx_create(ndpp_dev, dma_channel);
    ndpp_rx_t* rx_chn = ndpp_rx_create(ndpp_dev, dma_channel);

    if (!tx_chn || !rx_chn) {
        std::cerr << "Failed to create TX or RX channel." << std::endl;
        if (tx_chn) ndpp_tx_destroy(tx_chn);
        if (rx_chn) ndpp_rx_destroy(rx_chn);
        ndpp_dev_destroy(ndpp_dev);
        return 1;
    }

    // Prepare Packets and Send
    auto start_time = std::chrono::high_resolution_clock::now();
    bool first_byte_received = false;

    // Matrix 1
    for (int i = 0; i < TOTAL_PACKETS_PER_MATRIX; ++i) {
        int start_row = i * CHUNK_ROWS;
        std::vector<uint8_t> packet(PACKET_SIZE);

        // Header
        packet[0] = static_cast<uint8_t>(start_row);
        packet[1] = 1; // Matrix ID 1
        
        // Payload
        int payload_idx = 8;
        for (int r = start_row; r < start_row + CHUNK_ROWS; ++r) {
            const auto& row = (r < matrices[0].size()) ? matrices[0][r] : std::vector<int>();
            for (int c = 0; c < COLS; ++c) {
                uint8_t val = (c < row.size()) ? static_cast<uint8_t>(row[c]) : 0;
                packet[payload_idx++] = val;
            }
        }

        if (ndpp_transmit(tx_chn, packet.data(), packet.size(), 0) < 0) {
            std::cerr << "Tx Failed for Matrix 1 Packet " << i 
                      << " Error: " << ndpp_strerror(errno) << std::endl;
            goto cleanup;
        }
        // std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Matrix 2
    for (int i = 0; i < TOTAL_PACKETS_PER_MATRIX; ++i) {
        int start_row = i * CHUNK_ROWS;
        std::vector<uint8_t> packet(PACKET_SIZE);

        // Header
        packet[0] = static_cast<uint8_t>(start_row);
        packet[1] = 2; // Matrix ID 2
        if (i == TOTAL_PACKETS_PER_MATRIX - 1) {
            packet[7] = 0xFF; // Trigger
        }

        // Payload
        int payload_idx = 8;
        for (int r = start_row; r < start_row + CHUNK_ROWS; ++r) {
            const auto& row = (r < matrices[1].size()) ? matrices[1][r] : std::vector<int>();
            for (int c = 0; c < COLS; ++c) {
                uint8_t val = (c < row.size()) ? static_cast<uint8_t>(row[c]) : 0;
                packet[payload_idx++] = val;
            }
        }

        if (ndpp_transmit(tx_chn, packet.data(), packet.size(), 0) < 0) {
            std::cerr << "Tx Failed for Matrix 2 Packet " << i 
                      << " Error: " << ndpp_strerror(errno) << std::endl;
            goto cleanup;
        }
        // std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Receive Logic
    {
        std::vector<uint8_t> recv_buf(RECV_TOTAL_SIZE);
        size_t total_received = 0;
        auto recv_start = std::chrono::steady_clock::now();
        
        while (total_received < RECV_TOTAL_SIZE) {
            // Receive into buffer at offset
            ssize_t ret = ndpp_receive_0(rx_chn, recv_buf.data() + total_received, RECV_TOTAL_SIZE - total_received, 0);
            
            if (ret > 0) {
                if (!first_byte_received) {
                    auto now = std::chrono::high_resolution_clock::now();
                    auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count();
                    std::cout << "Latency (First Tx -> First Rx): " << latency_us << " us (" 
                              << std::fixed << std::setprecision(3) << (latency_us / 1000.0) << " ms)" << std::endl;
                    first_byte_received = true;
                }
                total_received += ret;
            }

            // Timeout check (10 seconds)
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - recv_start).count() > 10) {
                std::cerr << "Timeout waiting for data." << std::endl;
                break;
            }
            
            if (ret <= 0) {
                 if (ret < 0) {
                     // Optionally print error if it's not just EAGAIN
                    std::cerr << "Receive error <ret less than 0> : " << ndpp_strerror(errno) << std::endl;
                 }
                 else if (ret == 0) {
                    std::cerr << "Receive error <ret == 0> : " << ndpp_strerror(errno) << std::endl;
                 }
                 std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }

        if (total_received >= RECV_TOTAL_SIZE) {
            std::cout << "Received full batch (" << total_received << " bytes)." << std::endl;
            
            // Process Output
            std::ofstream out_file(output_file);
            if (!out_file.is_open()) {
                std::cerr << "Error: Could not open output file " << output_file << std::endl;
            } else {
                // Parse Payload (skip 8 bytes header)
                const uint32_t* data_ptr = reinterpret_cast<const uint32_t*>(recv_buf.data() + RECV_HEADER_SIZE);
                
                // Assuming Little Endian (x86 is LE, Python struct.unpack('<') is LE)
                // If the host is LE, we can just cast. If BE, we might need swap.
                // We assume host is LE (Linux x86/ARM usually).
                
                for (int r = 0; r < ROWS; ++r) {
                    for (int c = 0; c < COLS; ++c) {
                        uint32_t val = data_ptr[r * COLS + c];
                        out_file << val;
                        if (c < COLS - 1) out_file << ",";
                    }
                    out_file << "\n";
                }
                out_file.close();
            }
        } else {
            std::cerr << "Failed to receive full batch. Received: " << total_received << std::endl;
        }
    }

cleanup:
    if (tx_chn) ndpp_tx_destroy(tx_chn);
    if (rx_chn) ndpp_rx_destroy(rx_chn);
    if (ndpp_dev) ndpp_dev_destroy(ndpp_dev);

    return 0;
}