#!/bin/bash
gst-launch-1.0 v4l2src ! video/x-raw,height=1080,width=1920 ! videoconvert ! x264enc bitrate=8000 tune=zerolatency speed-preset=superfast byte-stream=true threads=4 key-int-max=15 intra-refresh=true ! video/x-h264, profile=baseline ! mpegtsmux ! srtsink uri=srt://0.0.0.0:8888?mode=listener latency=0
