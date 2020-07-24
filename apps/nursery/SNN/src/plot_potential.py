import matplotlib.pyplot as plt
import numpy as np
import matplotlib
import sys

sys.stderr.write("Starting\n")

rows=[]
for line in sys.stdin:
    cols=[float(x) for x in line.split(',')]
    rows.append(cols)

sys.stderr.write("Converting to bitmap")
bm=np.zeros( (len(rows),len(rows[0])), dtype=np.double )
for i in range(len(rows)):
    for j in range(len(rows[i])):
        bm[i,j]=rows[i][j]

sys.stderr.write("Plotting\n")
plt.imshow(bm, cmap='gray')
sys.stderr.write("Showing\n")
plt.colorbar()
plt.show()