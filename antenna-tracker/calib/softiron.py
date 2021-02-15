import csv
import sys

reader = csv.reader(iter(sys.stdin.readline, ""), delimiter=",")
data = list(reader)

x = [float(row[0]) for row in data]
y = [float(row[1]) for row in data]
z = [float(row[2]) for row in data]

avg_delta_x = (max(x) - min(x)) / 2
avg_delta_y = (max(y) - min(y)) / 2
avg_delta_z = (max(z) - min(z)) / 2

avg_delta = (avg_delta_x + avg_delta_y + avg_delta_z) / 3

scale_x = avg_delta / avg_delta_x
scale_y = avg_delta / avg_delta_y
scale_z = avg_delta / avg_delta_z

for row in data:
    corrected_x = float(row[0]) * scale_x
    corrected_y = float(row[1]) * scale_y
    corrected_z = float(row[2]) * scale_z

    print(",".join(str(value) for value in [corrected_x, corrected_y, corrected_z]))

#print(scale_x, scale_y, scale_z)
