python3 -c "
import random
path = '/root/DOWNLOAD/SWIFT/pkg-20260220/pkg/scirpt_recv/input-matrix-random.csv'
with open(path, 'w') as f:
    for _ in range(256): f.write(','.join(str(random.randint(0, 255)) for _ in range(256)) + '\n')
    f.write('\n')
    for _ in range(256): f.write(','.join(str(random.randint(0, 255)) for _ in range(256)) + '\n')
print('Successfully generated two 256x256 random matrices!')
"
