#pragma once

extern "C" {
  #include <assert.h>
  #include <errno.h>
  #include <fcntl.h>
  #include <getopt.h>
  #include <pthread.h>
  #include <signal.h>
  #include <stdbool.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <sys/poll.h>
  #include <time.h>
  #include <unistd.h>
}

#include <iostream>
#include <atomic>
#include <stdexcept>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "luckfox_mpi.h"

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

struct video_data{
  uint8_t *pData;
  size_t size;
  uint64_t frame_pts;
};


class VideoServer
{
private:
  RK_S32 s32Ret;
  VENC_STREAM_S stFrame;
  VIDEO_FRAME_INFO_S stViFrame;
  VIDEO_FRAME_INFO_S enFrame;
  cv::Mat frame;
  MB_POOL_CONFIG_S PoolCfg;
  MB_POOL src_Pool;
  MB_BLK src_Blk;
  RK_BOOL multi_sensor;
  char iq_dir[16];
  int width;
  int height;
  unsigned char *data;
  char fps_text[16];
  float fps = 0;
  std::atomic<bool> running;
  std::mutex frame_mutex;
  video_data video_ready_data;
  std::condition_variable video_data_cv;
  std::thread worker_thread;

  void _video_worker_cb();
public:
  VideoServer(int w = 640, int h = 480, RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_AVC);
  ~VideoServer();
  video_data get_video_data();
  bool isRunning(){return running.load();};
};