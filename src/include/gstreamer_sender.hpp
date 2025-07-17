#pragma once
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

class GStreamerSender {
public:
    GStreamerSender(const char* host, int port) {
        gst_init(nullptr, nullptr);

        pipeline = gst_parse_launch(
            "appsrc name=mysrc is-live=true do-timestamp=true format=time "
             "! h264parse config-interval=1 "
             "! flvmux streamable=true "
             "! udpsink host=192.168.50.107 port=5000",
            nullptr);

        appsrc = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(pipeline), "mysrc"));
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        g_object_set(appsrc, "emit-signals", FALSE, "format", GST_FORMAT_TIME, "is-live", TRUE, "do-timestamp", TRUE, NULL);
        g_object_set(appsrc, "caps",
            gst_caps_from_string("video/x-h264,stream-format=byte-stream,alignment=au"),
            NULL);
    }

    ~GStreamerSender() {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    }

    void pushFrame(const uint8_t* data, size_t size, uint64_t pts_usec) {
        GstBuffer* buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
        gst_buffer_fill(buffer, 0, data, size);

        GST_BUFFER_PTS(buffer) = pts_usec * 1000; // microsegundos a nanosegundos
        GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, 30); // 30 fps

        gst_app_src_push_buffer(appsrc, buffer);
    }

private:
    GstElement* pipeline;
    GstAppSrc* appsrc;
};
