#include "video_server.hpp"

VideoServer::VideoServer(int width, int height, RK_CODEC_ID_E enCodecType){
  // Clear fps_text mem
  memset(fps_text, 0, 16);

  // Frame initialization
  stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));

  // Create Pool
  memset(&PoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
  PoolCfg.u64MBSize = width * height * 3;
  PoolCfg.u32MBCnt = 1;
  PoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
  // PoolCfg.bPreAlloc = RK_FALSE;
  src_Pool = RK_MPI_MB_CreatePool(&PoolCfg);
  printf("Create Pool success !\n");

  // Get MB from Pool
  src_Blk = RK_MPI_MB_GetMB(src_Pool, width * height * 3, RK_TRUE);

  // Build enFrame
  enFrame.stVFrame.u32Width = width;
  enFrame.stVFrame.u32Height = height;
  enFrame.stVFrame.u32VirWidth = width;
  enFrame.stVFrame.u32VirHeight = height;
  enFrame.stVFrame.enPixelFormat = RK_FMT_RGB888;
  enFrame.stVFrame.u32FrameFlag = 160;
  enFrame.stVFrame.pMbBlk = src_Blk;
  data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_Blk);
  frame = cv::Mat(cv::Size(width,height),CV_8UC3,data);

  // rkaiq init
  multi_sensor = RK_FALSE;
  snprintf(iq_dir, sizeof(iq_dir), "/etc/iqfiles");
  rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
  // hdr_mode = RK_AIQ_WORKING_MODE_ISP_HDR2;
  SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
  SAMPLE_COMM_ISP_Run(0);

  // rkmpi init
  if (RK_MPI_SYS_Init() != RK_SUCCESS){
    throw std::runtime_error( "rk mpi sys init fail!" );
  }

  // vi init
  vi_dev_init();
  vi_chn_init(0, width, height);

  // venc init
  venc_init(0, width, height, enCodecType);

  printf("init success\n");

  // start worker
  running = true;
  worker_thread = std::thread(&VideoServer::_video_worker_cb, this);
}

void VideoServer::_video_worker_cb(){
    while (running.load()) {
        enFrame.stVFrame.u32TimeRef++;
        enFrame.stVFrame.u64PTS = TEST_COMM_GetNowUs();
        // s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1);
        // if (s32Ret != RK_SUCCESS) continue;

        // void *vi_data = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk);
        // if (!vi_data) continue;

        // cv::Mat yuv420sp(height + height / 2, width, CV_8UC1, vi_data);
        // if (!data) {
        //   printf("data is null\n");
        //   continue;
        // }
        // cv::Mat bgr(height, width, CV_8UC3, data);			
        // cv::Mat bgr_tmp;
        // printf("1####\n");
        // cv::cvtColor(yuv420sp, bgr_tmp, cv::COLOR_YUV420sp2BGR);
        // printf("2####\n");
        // cv::resize(bgr_tmp, frame, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);
        // printf("3####\n");
        
        // sprintf(fps_text,"fps = %.2f",fps);		
        //       cv::putText(frame,fps_text,
        //         cv::Point(40, 40),
        //         cv::FONT_HERSHEY_SIMPLEX,1,
        //         cv::Scalar(0,255,0),2);

        // memcpy(data, frame.data, width * height * 3);

        RK_MPI_VENC_SendFrame(0, &enFrame, -1);

        s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, -1);
        if (s32Ret == RK_SUCCESS) {
            void *pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
            if (!pData) continue;

            auto size = stFrame.pstPack->u32Len;
            auto frame_pts = stFrame.pstPack->u64PTS;

            std::lock_guard<std::mutex> lock(frame_mutex);
            free(video_ready_data.pData);
            video_ready_data.pData = (uint8_t*)malloc(size);
            memcpy(video_ready_data.pData, pData, size);
            video_ready_data.size = size;
            video_ready_data.frame_pts = frame_pts;

            video_data_cv.notify_all();
            RK_U64 nowUs = TEST_COMM_GetNowUs();
            fps = 1000000.0f / (nowUs - enFrame.stVFrame.u64PTS);
        }

        // RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
        RK_MPI_VENC_ReleaseStream(0, &stFrame);
    }
}

video_data VideoServer::get_video_data(){
  std::lock_guard<std::mutex> lock(frame_mutex);
  return video_ready_data;
}

VideoServer::~VideoServer(){
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
  running = false;
  if (worker_thread.joinable())
    worker_thread.join();
  
  printf("Video closed!\n");
};