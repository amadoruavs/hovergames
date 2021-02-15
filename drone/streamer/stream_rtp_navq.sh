#!/bin/bash
gst-launch-1.0 v4l2src ! video/x-raw,height=1080,width=1920 ! videoconvert ! vpuenc_vp8 ! rtpvp8pay ! udpsink host=172.16.0.100 port=9001
