import csv

def generate_csv():
    filename = 'input-matrix.csv'
    
    with open(filename, 'w', newline='') as f:
        writer = csv.writer(f)
        
        # Write Matrix A (256 rows)
        # Row 0: All 0, Row 1: All 1 ... Row 255: All 255
        for r in range(256):
            row_data = [r] * 256
            writer.writerow(row_data)
            
        # Write Matrix B (256 rows)
        # Same pattern: Row 0: All 0 ... Row 255: All 255
        for r in range(256):
            row_data = [r] * 256
            writer.writerow(row_data)

    print(f"Generated {filename} with two 256x256 matrices (Row value = Row index).")

if __name__ == "__main__":
    generate_csv()