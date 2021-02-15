#!/bin/bash
gst-launch-1.0 udpsrc port=9001 caps="application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)VP8-DRAFT-IETF-01, payload=(int)96, ssrc=(uint)2990747501, clock-base=(uint)275641083, seqnum-base=(uint)34810" ! rtpvp8depay ! vp8dec ! ffmpegcolorspace ! autovideosink
