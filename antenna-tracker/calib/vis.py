import csv
import sys
from matplotlib import pyplot as plt
from mpl_toolkits.mplot3d import axes3d

reader = csv.reader(iter(sys.stdin.readline, ""), delimiter=",")
data = list(reader)

x = [float(row[0]) for row in data]
y = [float(row[1]) for row in data]
z = [float(row[2]) for row in data]

fig = plt.figure()
ax = plt.axes(projection="3d")

# ax.plot3D(x, y, z)
ax.scatter(x, y, z)
ax.set_xlabel("X")
ax.set_ylabel("Y")
ax.set_zlabel("Z")

plt.show()
