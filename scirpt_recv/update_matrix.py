def update_matrix():
    matrix1 = []
    # Matrix 1: 128 rows, each row has value = (row_index + 1) % 16
    for i in range(128):
        val = i
        # If line number is 16, val is 0.
        row = [str(val)] * 128
        matrix1.append(",".join(row))

    matrix2 = []
    # Matrix 2: 128 rows, placeholder value 1
    for i in range(128):
        row = ["1"] * 128
        matrix2.append(",".join(row))

    with open("/root/DOWNLOAD/SWIFT/pkg-20260220/pkg/scirpt_recv/input-matrix.csv", "w") as f:
        f.write("\n".join(matrix1 + matrix2))
    
    print("Updated input-matrix.csv successfully.")

if __name__ == "__main__":
    update_matrix()