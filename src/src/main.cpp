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
#include <rtc/rtc.hpp>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>

#include "luckfox_mpi.h"
#include "web_rtc_streamer.hpp"

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#define DISP_WIDTH 640
#define DISP_HEIGHT 480

std::mutex frame_mutex;
struct video_data{
  uint8_t *pData;
  size_t size;
  uint64_t frame_pts;
};

video_data video_ready_data;

int main(int argc, char *argv[])
{
  system("RkLunch-stop.sh");
  WebRTCServer ws_server(9000, "/root/cert.pem", "/root/key.pem");

  RK_S32 s32Ret = 0;

  int width = DISP_WIDTH;
  int height = DISP_HEIGHT;

  char fps_text[16];
  float fps = 0;
  memset(fps_text, 0, 16);

  // enc_frame
  VENC_STREAM_S stFrame;
  stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
  RK_U64 H264_PTS = 0;
  RK_U32 H264_TimeRef = 0;
  VIDEO_FRAME_INFO_S stViFrame;

  // Create Pool
  MB_POOL_CONFIG_S PoolCfg;
  memset(&PoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
  PoolCfg.u64MBSize = width * height * 3;
  PoolCfg.u32MBCnt = 1;
  PoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
  // PoolCfg.bPreAlloc = RK_FALSE;
  MB_POOL src_Pool = RK_MPI_MB_CreatePool(&PoolCfg);
  printf("Create Pool success !\n");

  // Get MB from Pool
  MB_BLK src_Blk = RK_MPI_MB_GetMB(src_Pool, width * height * 3, RK_TRUE);

  // Build enc_frame
  VIDEO_FRAME_INFO_S enc_frame;
  enc_frame.stVFrame.u32Width = width;
  enc_frame.stVFrame.u32Height = height;
  enc_frame.stVFrame.u32VirWidth = width;
  enc_frame.stVFrame.u32VirHeight = height;
  enc_frame.stVFrame.enPixelFormat = RK_FMT_RGB888;
  enc_frame.stVFrame.u32FrameFlag = 160;
  enc_frame.stVFrame.pMbBlk = src_Blk;
  unsigned char *data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_Blk);
  cv::Mat frame(cv::Size(width, height), CV_8UC3, data);

  // rkaiq init
  RK_BOOL multi_sensor = RK_FALSE;
  const char *iq_dir = "/etc/iqfiles";
  rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
  // hdr_mode = RK_AIQ_WORKING_MODE_ISP_HDR2;
  SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
  SAMPLE_COMM_ISP_Run(0);

  // rkmpi init
  if (RK_MPI_SYS_Init() != RK_SUCCESS)
  {
    RK_LOGE("rk mpi sys init fail!");
    return -1;
  }

  // vi init
  vi_dev_init();
  vi_chn_init(0, width, height);

  // venc init
  RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_AVC;
  venc_init(0, width, height, enCodecType);

  printf("init success\n");

  uint32_t size = 0;
  uint64_t frame_pts = 0;

  while (1)
  {
    // get vi frame
    enc_frame.stVFrame.u32TimeRef = H264_TimeRef++;
    enc_frame.stVFrame.u64PTS = TEST_COMM_GetNowUs();
    s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1);
    if (s32Ret == RK_SUCCESS)
    {
      void *vi_data = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk);

      cv::Mat yuv420sp(height + height / 2, width, CV_8UC1, vi_data);
      cv::Mat bgr(height, width, CV_8UC3, data);
      cv::cvtColor(yuv420sp, bgr, cv::COLOR_YUV420sp2BGR);
      cv::resize(bgr, frame, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);

      sprintf(fps_text, "fps = %.2f", fps);
      cv::putText(frame, fps_text,
                  cv::Point(40, 40),
                  cv::FONT_HERSHEY_SIMPLEX, 1,
                  cv::Scalar(0, 255, 0), 2);
    }
    memcpy(data, frame.data, width * height * 3);

    // encode H264
    RK_MPI_VENC_SendFrame(0, &enc_frame, -1);

    s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, -1);
    if (s32Ret == RK_SUCCESS)
    {
      {
        // printf("len = %d PTS = %d \n",stFrame.pstPack->u32Len, stFrame.pstPack->u64PTS);
        void *pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
        size = stFrame.pstPack->u32Len;
        frame_pts = stFrame.pstPack->u64PTS;
        // std::lock_guard<std::mutex> lock(frame_mutex);
        // free(video_ready_data.pData);
        // video_ready_data.pData = (uint8_t*)malloc(size);
        // memcpy(video_ready_data.pData, pData, size);
        // video_ready_data.size = size;
        // video_ready_data.frame_pts = frame_pts;
        // ws_server.send_frame(static_cast<const uint8_t*>(pData), size, frame_pts);
        ws_server.send_frame(static_cast<const uint8_t*>(pData), (size_t)size);
      }
      RK_U64 nowUs = TEST_COMM_GetNowUs();
      fps = (float)1000000 / (float)(nowUs - enc_frame.stVFrame.u64PTS);
    }

    // release frame
    s32Ret = RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
    if (s32Ret != RK_SUCCESS)
    {
      RK_LOGE("RK_MPI_VI_ReleaseChnFrame fail %x", s32Ret);
    }
    s32Ret = RK_MPI_VENC_ReleaseStream(0, &stFrame);
    if (s32Ret != RK_SUCCESS)
    {
      RK_LOGE("RK_MPI_VENC_ReleaseStream fail %x", s32Ret);
    }
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Destory MB
  RK_MPI_MB_ReleaseMB(src_Blk);
  // Destory Pool
  RK_MPI_MB_DestroyPool(src_Pool);

  RK_MPI_VI_DisableChn(0, 0);
  RK_MPI_VI_DisableDev(0);

  SAMPLE_COMM_ISP_Stop(0);

  RK_MPI_VENC_StopRecvFrame(0);
  RK_MPI_VENC_DestroyChn(0);

  free(stFrame.pstPack);

  RK_MPI_SYS_Exit();

  return 0;
}
