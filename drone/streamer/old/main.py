import gi 
gi.require_version("Gst", "1.0")
from gi.repository import Gst, GObject
import time

Gst.init(None)

pipeline = Gst.Pipeline()

video_src = Gst.ElementFactory.make("v4l2src")
video_src.set_property("device", "/dev/video0")
record_caps = Gst.caps_from_string("video/x-bayer,framerate=60/1,width=1920,height=1080")
record_caps_filter = Gst.ElementFactory.make("capsfilter")
record_caps_filter.set_property("caps", record_caps)
record_rgb = Gst.ElementFactory.make("bayer2rgb")

tee = Gst.ElementFactory.make("tee", "tee")
stream_queue = Gst.ElementFactory.make("queue", "stream_queue")
stream_queue.set_property("leaky", 1)
record_queue = Gst.ElementFactory.make("queue", "record_queue")

pipeline.add(video_src)
pipeline.add(record_caps_filter)
pipeline.add(record_rgb)
pipeline.add(tee)
pipeline.add(stream_queue)
pipeline.add(record_queue)

video_src.link(record_caps_filter)
record_caps_filter.link(record_rgb)
record_rgb.link(tee)
tee.link(stream_queue)
tee.link(record_queue)

# Hook up stream
stream_converter = Gst.ElementFactory.make("nvvidconv")
stream_scaler = Gst.ElementFactory.make("videoscale")
stream_caps = Gst.caps_from_string("video/x-raw(memory:NVMM),width=1920,height=1080")
stream_caps_filter = Gst.ElementFactory.make("capsfilter")
stream_caps_filter.set_property("caps", stream_caps)
stream_encoder = Gst.ElementFactory.make("nvv4l2vp8enc")
stream_payload = Gst.ElementFactory.make("rtpvp8pay")

udp_sink = Gst.ElementFactory.make("udpsink")
udp_sink.set_property("host", "172.16.0.138")
udp_sink.set_property("port", 5100)

pipeline.add(stream_converter)
pipeline.add(stream_scaler)
pipeline.add(stream_caps_filter)
pipeline.add(stream_encoder)
pipeline.add(stream_payload)
pipeline.add(udp_sink)

stream_queue.link(stream_converter)
stream_converter.link(stream_scaler)
stream_scaler.link(stream_caps_filter)
stream_caps_filter.link(stream_encoder)
stream_encoder.link(stream_payload)
stream_payload.link(udp_sink)

# Hook up record
record_converter = Gst.ElementFactory.make("nvvidconv")
record_nvcaps = Gst.caps_from_string("video/x-raw(memory:NVMM)")
record_nvcaps_filter = Gst.ElementFactory.make("capsfilter")
record_encoder = Gst.ElementFactory.make("nvv4l2vp8enc")
record_muxer = Gst.ElementFactory.make("matroskamux")
file_sink = Gst.ElementFactory.make("filesink")
file_sink.set_property("location", "log.mkv")

pipeline.add(record_converter)
pipeline.add(record_nvcaps_filter)
pipeline.add(record_encoder)
pipeline.add(record_muxer)
pipeline.add(file_sink)

record_queue.link(record_converter)
record_converter.link(record_nvcaps_filter)
record_nvcaps_filter.link(record_encoder)
record_encoder.link(record_muxer)
record_muxer.link(file_sink)

pipeline.set_state(Gst.State.PLAYING)

input()

pipeline.send_event(Gst.Event.new_eos())
pipeline.set_state(Gst.State.NULL)
