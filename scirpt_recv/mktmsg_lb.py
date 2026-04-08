# -*- coding:utf-8 -*-
# @Author: Yusur
# @Date: 2024/11/13 15:22
# @File: mktmsg_lb.py
import json
import socket
import sys
import signal
import time
import re
import struct
from yusur_ndpp_impl import YusurNdppImpl

dev_name = "swiftn0"
 
def parse_matrices(file_path):
    matrices = []
    current_matrix = []
    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                if current_matrix:
                    matrices.append(current_matrix)
                    current_matrix = []
                continue
            row = []
            tokens = line.split(',')
            for token in tokens:
                token = token.strip()
                if not token: continue
                try:
                    val = int(token)
                    row.append(val)
                except ValueError:
                    pass
            if row:
                current_matrix.append(row)
    if current_matrix:
        matrices.append(current_matrix)
    return matrices

def format_output_data(data):
    tokens = []
    for byte in data:
        if byte == 254:
            tokens.append('|')
        elif byte == 255:
            tokens.append('@')
        else:
            tokens.append(str(byte))
    return ",".join(tokens)

def run_test_loop(input_file, output_file):
    print(f"Running test loop with input file: {input_file}, output file: {output_file}")
    
    ndpp_impl = YusurNdppImpl()   
    ndpp_dev = ndpp_impl.yusur_ndpp_dev_create(dev_name)
    
    if ndpp_dev is None:
        print("Failed to initialize NDPP device.")
        return

    # Soft reset
    if ndpp_impl.yusur_ndpp_register_write32(ndpp_dev, 0x0000, 0x01) < 0:
        print(f"Failed to write to registers: {ndpp_impl.get_current_ndpp_error_msg()}")
        ndpp_impl.yusur_ndpp_dev_destroy(ndpp_dev)
        return
    
    print("yusur_ndpp_reset successful.")
    time.sleep(1)

    try:
        # Create TX and RX channels on DMA channel 0
        dma_channel = 0
        tx_chn = ndpp_impl.yusur_ndpp_tx_create(ndpp_dev, dma_channel)
        rx_chn = ndpp_impl.yusur_ndpp_rx_create(ndpp_dev, dma_channel)
        
        if tx_chn is None or rx_chn is None:
            print("Failed to create TX or RX channel")
            ndpp_impl.yusur_ndpp_dev_destroy(ndpp_dev)
            return

        matrices = parse_matrices(input_file)
        if len(matrices) != 2:
            print(f"Error: Expected 2 matrices, found {len(matrices)}")
            return

        m1 = matrices[0]
        m2 = matrices[1]
        
        rows1 = len(m1)
        cols1 = len(m1[0]) if rows1 > 0 else 0
        rows2 = len(m2)
        cols2 = len(m2[0]) if rows2 > 0 else 0
        
        if rows1 > 256 or cols1 > 256:
            print(f"ERROR: 1st matrix size ({rows1}x{cols1}) exceeds 256x256.")
            return
        if rows2 > 256 or cols2 > 256:
            print(f"ERROR: 2nd matrix size ({rows2}x{cols2}) exceeds 256x256.")
            return

        # Helper to get 16 rows x 256 cols (4096 items) as uint8
        def get_chunk_data(matrix, start_row):
            chunk = bytearray()
            rows_to_pack = 16
            cols_per_row = 256
            
            for r in range(start_row, start_row + rows_to_pack):
                row_data = matrix[r] if r < len(matrix) else []
                # Pad row to 256 items
                if len(row_data) < cols_per_row:
                    row_data.extend([0] * (cols_per_row - len(row_data)))
                
                # Append uint8 items
                for c in range(cols_per_row):
                    val = row_data[c]
                    if val > 255:
                        # print(f"Warning: Value {val} > 255, truncating.")
                        val = 255
                    chunk.append(val)
            return chunk

        with open(output_file, 'w') as f_out:
            # Measure Total Time
            total_start_time = time.time()
            first_byte_received = False
            
            # Send 32 Packets (16 for Matrix 1, 16 for Matrix 2)
            # Matrix 1
            for i in range(16):
                start_row = i * 16
                
                # Header: 0x000000000000{01}{row_start} (Little Endian Layout for AXI)
                # Byte 0: Row Start (Bits 0-7)
                # Byte 1: Matrix ID (Bits 8-15)
                # Byte 7: Trigger (Bits 56-63)
                header = bytearray(8)
                header[1] = 1         # Matrix ID 1
                header[0] = start_row # Start Row
                
                payload = get_chunk_data(m1, start_row)
                pkt = header + payload
                
                if len(pkt) != 4104:
                    print(f"Error: Packet M1-{i} length {len(pkt)} != 4104")
                    return
                
                #print(f"Sending Matrix 1 Packet {i+1}/16 (Row {start_row}), len: {len(pkt)}...")
                #if i>10 :   
                ret = ndpp_impl.yusur_ndpp_transmit(tx_chn, pkt, len(pkt), 0)
                if ret < 0:
                        err, msg = ndpp_impl.get_current_ndpp_error_msg()
                        print(f"Tx Failed: {err}, {msg}")
                        #return

            # Matrix 2
            for i in range(16):
                start_row = i * 16
                
                # Header: 0x000000000000{02}{row_start} (Little Endian Layout)
                header = bytearray(8)
                header[1] = 2         # Matrix ID 2
                header[0] = start_row # Start Row
                
                # Last packet of 2nd matrix (i=15) -> Set top byte (Byte 7) to 0xFF
                if i == 15:
                    header[7] = 0xFF

                payload = get_chunk_data(m2, start_row)
                
                pkt = header + payload
                
                if len(pkt) != 4104:
                    print(f"Error: Packet M2-{i} length {len(pkt)} != 4104")
                    return

                #print(f"Sending Matrix 2 Packet {i+1}/16 (Row {start_row}), len: {len(pkt)}...")
                #if i==15:
                ret = ndpp_impl.yusur_ndpp_transmit(tx_chn, pkt, len(pkt), 0)
                if ret < 0:
                        err, msg = ndpp_impl.get_current_ndpp_error_msg()
                        print(f"Tx Failed: {err}, {msg}")
                        return
                # Small delay between packets
                #time.sleep(0.005)

            # Receive packets (Expect 1 batch of 262152 bytes)
            #print("Waiting for response (Expect 1 batch of 262152 bytes)...")
            
            total_batches = 1
            batch_size = 262152
            
            for batch_idx in range(total_batches):
                #print(f"--- Receiving Batch {batch_idx + 1}/{total_batches} ---")
                
                batch_data = bytearray()
                
                # Loop to receive full batch
                start_time = time.time()
                while len(batch_data) < batch_size:
                    # Attempt to read the full expected size
                    temp_buf = bytearray(batch_size) 
                    result = ndpp_impl.yusur_ndpp_receive(rx_chn, temp_buf, len(temp_buf), 0) 
                    
                    if result > 0:
                        if not first_byte_received:
                            total_end_time = time.time()
                            latency_ms = (total_end_time - total_start_time) * 1000
                            print(f"Latency (First Tx -> First Rx): {latency_ms:.2f} ms")
                            first_byte_received = True
                            
                        batch_data.extend(temp_buf[:result])
                        #print(f"Received {result} bytes. Total: {len(batch_data)}/{batch_size}")
                        if len(batch_data) >= batch_size:
                            break
                    #else:
                        #time.sleep(0.001)
                        
                    if time.time() - start_time > 10: # 10s timeout
                        print("Timeout waiting for data.")
                        break
                
                #print(f"Batch {batch_idx + 1} received {len(batch_data)} bytes.")

                # Process the batch
                current_batch = batch_data[:batch_size]
                
                # 1. Header (8 bytes)
                header = current_batch[:8]
                # Convert header to hex string for display
                header_hex = "0x" + "".join(f"{b:02x}" for b in reversed(header)) # Little Endian display? Or Big?
                
                # 2. Data (32768 bytes -> 128 rows * 128 cols * 2 bytes)
                raw_data = current_batch[8:262152]
                try:
                    # Unpack 65536 uint32 values (Little Endian)
                    # 256 * 256 = 65536
                    num_vals = len(raw_data) // 4
                    vals = struct.unpack(f'<{num_vals}I', raw_data)
                    
                    # 256 rows
                    rows_per_batch = 256
                    cols_per_row = 256
                    
                    for r in range(rows_per_batch):
                        start_idx = r * cols_per_row
                        end_idx = start_idx + cols_per_row
                        row_vals = vals[start_idx:end_idx]
                        
                        # Format as comma-separated string
                        row_str = ",".join(str(v) for v in row_vals)
                        # print(f"Row {r}: {row_str[:50]}...") # Print preview
                        f_out.write(row_str + "\n")
                        
                except Exception as e:
                    print(f"Error unpacking batch {batch_idx + 1}: {e}")
                    f_out.write(f"Error unpacking batch {batch_idx + 1}: {e}\n")
                
    except Exception as e:
        print(f"Error: {e}")
    finally:
        ndpp_impl.yusur_ndpp_dev_destroy(ndpp_dev)

if __name__ == "__main__":
    if len(sys.argv) > 2:
        run_test_loop(sys.argv[1], sys.argv[2])
    else:
        print("Usage: python mktmsg_lb.py <input_file> <output_file>")
