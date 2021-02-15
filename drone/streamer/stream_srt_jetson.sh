#!/bin/bash
GST_DEBUG=srt*:5 gst-launch-1.0 v4l2src ! 'video/x-raw,height=1080,width=1920,framerate=30/1' ! videoconvert ! omxh264enc ! mpegtsmux ! srtsink uri=srt://0.0.0.0:8888?mode=listener latency=0
