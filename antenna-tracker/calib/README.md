This file contains a few scripts for magnetometer calibration.
Currently, the magnetometer calibration method (for soft iron errors)
is suboptimal and should be replaced with an ellipsoid fit in the future.

## Sampling
Calibration sequence is as follows:
Enable printing of raw magnetometer data from in src/mag.c. Deploy and connect to the zephyr console. Then, use an app to save a few thousand magnetometer readings. Command to log using `screen`:
```
$ screen -L -Logfile mag1.csv /dev/ttyUSB0 115200
```
Make sure to rotate the gyroscope in a full circle on all axes to capture all
range of values.

## Fitting
`vis.py` can be used to visualize the gathered samples: `vis.py < mag1.csv` will
open up a graph. Use this to make sure that the gyro values are reasonable. They
should represent (in a perfect world) a sphere or (in reality) some sort of
ellipsoid. If the shape appears to have a high number of outliers or other
deformities, check your sensor and environment, sanitize the data in software,
or redo the calibration.
### Hard Iron
The main source of errors from a magnetometer are "hard iron" errors, which
result in an offset to the center of the magnetometer (in a perfect world, it
would be centered around 0). `hardiron.py` aims to solve this.
Open `hardiron.py` and uncomment the line at the bottom that enables printing of
the offsets. Then, run `hardiron.py < mag1.csv` and keep note of the 3 values on
the last line of the output. This will be the offset in X, Y, and Z to apply to
the magnetometer. Then, comment the line again, and run `hardiron.py` again,
this time redirecting the output to a file: `hardiron.py < mag1.csv >
hardiron1.csv`. Run the visualization script on the hard iron-corrected data
file, which should now show an ellipsoid centered around 0.

### Soft Iron
The other source of errors from a magnetometer are "soft iron" errors, which
result in the data being an ellipsoid instead of a perfect sphere. While the
"proper" way to solve this problem is by using a linear algebra based ellipsoid
fitting algorithm, `softiron.py` takes a far simpler approach that yields
reasonable results. Open the script and uncomment the line at the bottom that
enables printing of the offsets. Run the script: `softiron.py < hardiron1.csv`
and keep note of the 3 values on the last line of the output. This will be the
scale in X, Y, and Z to apply to the magnetometer. Then, comment the line again,
and run `softiron.py` again, this time redirecting the output to a file:
`sofiron.py < hardiron1.csv > softiron1.csv`. Run the visualization script on
the soft iron-corrected data file, which should now show an object that is
reasonable spherical.

### Updating Antenna-Tracker
As of time of writing, the magnetometer code uses hard coded calibration
constants that are applied to each value. These can be found in include/mag.h.
Update them with the values you recorded from the above process, and you should
be good to go. Remember to redo the calibration process if your setup changes
(i.e. there is a metallic object near your magnetometer) or if the estimated
heading is very inaccurate.
