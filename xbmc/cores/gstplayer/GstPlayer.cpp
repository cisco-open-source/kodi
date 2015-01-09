

#include "GstPlayer.h"
#include "GstPlayBinManager.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/LangCodeExpander.h"

#ifdef HAS_VIDEO_PLAYBACK
#include "cores/VideoRenderers/RenderManager.h"
#include "cores/VideoRenderers/RenderFlags.h"
#endif
#include "settings/MediaSettings.h"
#include "settings/AdvancedSettings.h"

#include "dialogs/GUIDialogBusy.h"
#include "settings/Settings.h"
//Need to rework
#include "../dvdplayer/DVDCodecs/Video/DVDVideoCodec.h"



#define LOAD_TAG(data, tags, tagid) \
  do {\
    if ((data)==NULL){\
      gchar * value;\
      if (gst_tag_list_get_string ((tags), (tagid), &value)){\
        (data) = value;\
      }\
    }\
  }while(0)


CGstPlayer::CGstPlayer(IPlayerCallback& callback)
: IPlayer(callback),
  CThread("GstPlayer"),
  m_pPlayBin(NULL),
  m_pBus(NULL),
  videosink_handler(0),
  m_paused(false),
  m_speed(1),
  m_ready(true)
{
  printf("FUNCTION: %s\n", __FUNCTION__);
}

CGstPlayer::~CGstPlayer()
{
//TODO: need implement
  printf("FUNCTION: %s\n", __FUNCTION__);
  CloseFile();
}

bool CGstPlayer::OpenFile(const CFileItem& file, const CPlayerOptions& options)
{
  printf("FUNCTION: %s\n", __FUNCTION__);
  m_item = file;
  m_cancelled = false;

  memset(&m_mediainfo, 0, sizeof(MediaInfo));

  g_renderManager.PreInit();

  if (!InitializeGst())
    return false;
  // Create the player thread

  if(!CreatePlayBin())
    return false;

  m_ready.Reset();

//Start thread after playbin is created
  Create();

  CGUIDialogBusy::WaitOnEvent(m_ready, g_advancedSettings.m_videoBusyDialogDelay_ms, false);

  if(m_bStop)
    return false;

  m_callback.OnPlayBackStarted();
  return true;
}

bool CGstPlayer::CloseFile(bool)
{
  //TODO: need implement
  printf("FUNCTION: %s\n", __FUNCTION__);
  CLog::Log(LOGNOTICE, "CloseFile - Stopping Thread");
  //Stop thread playbin is destroyed playbin is created
  StopThread();

  if (m_pPlayBin)
    DestroyPlayBin();

  CleanMediaInfo();

  g_renderManager.UnInit();

  m_callback.OnPlayBackStopped();

  return true;
}

bool CGstPlayer::IsPlaying() const
{
  return !m_bStop;
}

void CGstPlayer::Pause()
{
  CLog::Log(LOGNOTICE, "---[%s]---", __FUNCTION__);

  m_paused = !m_paused;

  if (!m_paused) {
    if (SetAndWaitPlaybinState(GST_STATE_PLAYING, 10))
      m_callback.OnPlayBackResumed();
  }
  else {
    if (SetAndWaitPlaybinState(GST_STATE_PAUSED, 10))
      m_callback.OnPlayBackPaused();
  }
}


bool CGstPlayer::IsPaused() const
{
  return m_paused;
}

bool CGstPlayer::HasVideo() const
{
  return (m_mediainfo.video_num > 0);
}

bool CGstPlayer::HasAudio() const
{
  return (m_mediainfo.audio_num > 0);
}

bool CGstPlayer::CanSeek() 
{
  return m_mediainfo.seekable;
}

void CGstPlayer::Seek(bool bPlus, bool bLargeStep, bool bChapterOverride)
{
  int64_t seek;
  if (g_advancedSettings.m_videoUseTimeSeeking &&
      GetTotalTime() > 2*g_advancedSettings.m_videoTimeSeekForwardBig)
  {
    if (bLargeStep)
      seek = bPlus ? g_advancedSettings.m_videoTimeSeekForwardBig :
          g_advancedSettings.m_videoTimeSeekBackwardBig;
    else
      seek = bPlus ? g_advancedSettings.m_videoTimeSeekForward :
          g_advancedSettings.m_videoTimeSeekBackward;
    seek *= 1000;
    seek += GetTime();
  }
  else
  {
    float percent;
    if (bLargeStep)
      percent = bPlus ? g_advancedSettings.m_videoPercentSeekForwardBig :
          g_advancedSettings.m_videoPercentSeekBackwardBig;
    else
      percent = bPlus ? g_advancedSettings.m_videoPercentSeekForward :
          g_advancedSettings.m_videoPercentSeekBackward;
    seek = (int64_t)(GetTime()*(GetPercentage()+percent)/100);
  }

  GstEvent *seek_event;

  // m_clock = seek;
  if (m_pPlayBin){
    seek_event = gst_event_new_seek(1.0, GST_FORMAT_TIME,
        (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
        GST_SEEK_TYPE_SET, seek*1000000,
        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
    gst_element_send_event(m_pPlayBin, seek_event);
  }

  CLog::Log(LOGNOTICE, "---[%s finish]---%lld", __FUNCTION__, seek);
  // g_infoManager.m_performingSeek = false;
  int seekOffset = (int)(seek - GetTime());
  m_callback.OnPlayBackSeek((int)seek, seekOffset);
  CLog::Log(LOGNOTICE, "---[%s nretun]---%lld", __FUNCTION__, seek);
}

void CGstPlayer::SeekPercentage(float fPercent)
{
  printf("%s, fPercent = %4.2f\n", __FUNCTION__, fPercent);
  int64_t iTime = (int64_t)(GetTotalTime() * fPercent / 100);
  SeekTime(iTime);
}

void CGstPlayer::SeekTime(int64_t iTime)
{
  GstEvent *seek_event;
  int seekOffset = (int)(iTime - GetTime());
  CLog::Log(LOGNOTICE, "---[%s]---%lld", __FUNCTION__, iTime);

  // m_clock = iTime;
  if (m_pPlayBin){
    seek_event = gst_event_new_seek(1.0, GST_FORMAT_TIME,
        (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
        GST_SEEK_TYPE_SET, iTime*1000000,
        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
    gst_element_send_event(m_pPlayBin, seek_event);
  }

  CLog::Log(LOGNOTICE, "---[%s finish]---%lld", __FUNCTION__, iTime);
  m_callback.OnPlayBackSeek((int)iTime, seekOffset);
  CLog::Log(LOGNOTICE, "---[%s nretun]---%lld", __FUNCTION__, iTime);
}

float CGstPlayer::GetPercentage()
{
  int64_t iTotalTime = GetTotalTime();

  if (!iTotalTime)
    return 0.0f;

  return GetTime() * 100 / (float)iTotalTime;
}

void CGstPlayer::SetMute(bool bOnOff)
{
  if(m_pPlayBin)
    g_object_set(G_OBJECT(m_pPlayBin), "mute", bOnOff, NULL);
}

void CGstPlayer::SetVolume(float volume)
{
  if(m_pPlayBin)
    g_object_set(G_OBJECT(m_pPlayBin), "volume", volume, NULL);
}

bool CGstPlayer::ControlsVolume()
{
  return true;
}

void CGstPlayer::GetAudioInfo(std::string&)
{
  //TODO: need implement
  printf("FUNCTION: %s\n", __FUNCTION__);
}

void CGstPlayer::GetVideoInfo(std::string&)
{
  //TODO: need implement
  printf("FUNCTION: %s\n", __FUNCTION__);
}

void CGstPlayer::GetGeneralInfo(std::string&)
{
  //TODO: need implement
  printf("FUNCTION: %s\n", __FUNCTION__);
}

bool CGstPlayer::CanRecord()
{
  //TODO, maybe need implementation
  return false;
}

//TODO: Need to check if works
void CGstPlayer::SetAVDelay(float fValue)
{
  if ((m_pPlayBin) && (m_mediainfo.audio_num) && (m_mediainfo.video_num)){
    GstElement *videosink = NULL;
    GstElement *audiosink = NULL;
    guint64 value64 = 0;

    g_object_get( G_OBJECT(m_pPlayBin), "video-sink", &videosink, NULL);
    g_object_get( G_OBJECT(m_pPlayBin), "audio-sink", &audiosink, NULL);

    if ((videosink==NULL) || (audiosink==NULL)){
      goto bail;
    }

    if (fValue >= 0.0){
      SetSinkRenderDelay(videosink, value64);
      value64 = (guint64)(fValue*GST_SECOND);
      SetSinkRenderDelay(audiosink, value64);
    }else{
      SetSinkRenderDelay(audiosink, value64);
      value64 = (guint64)(-fValue*GST_SECOND);
      SetSinkRenderDelay(videosink, value64);
    }
bail:
    if (videosink){
      g_object_unref(G_OBJECT(videosink));
    }
    if (audiosink){
      g_object_unref(G_OBJECT(audiosink));
    }
  }
}

float CGstPlayer::GetAVDelay()
{
  printf("FUNCTION: %s\n", __FUNCTION__);
  return 0.0f;
}

int CGstPlayer::GetAudioStreamCount() 
{
  return m_mediainfo.audio_num;
}

int CGstPlayer::GetAudioStream()
{
  return m_audio_current;
}

void CGstPlayer::SetAudioStream(int iStream)
{
  if (m_pPlayBin && (iStream >= 0) && (iStream < m_mediainfo.audio_num)){
    m_audio_current = iStream;
    g_object_set( G_OBJECT(m_pPlayBin), "current-audio", iStream, NULL);
  }
}

void CGstPlayer::GetAudioStreamInfo(int index, SPlayerAudioStreamInfo &info)
{
  if (index < 0 || index > GetAudioStreamCount() - 1 )
    return;

  std::string temp;
  if (m_pPlayBin && m_mediainfo.audio_info && (index < m_mediainfo.audio_num)){
    if (m_mediainfo.audio_info[index].lang){
      std::string code = m_mediainfo.audio_info[index].lang;
      if (!g_LangCodeExpander.Lookup(temp, code))
        info.language = m_mediainfo.audio_info[index].lang;
    }
    if (m_mediainfo.audio_info[index].codec){
      info.audioCodecName = m_mediainfo.audio_info[index].codec;
    }
  }
}



int64_t CGstPlayer::GetTime()
{
  int64_t time = 0;
  gint64 elapsed = 0;
  if (m_pPlayBin) {
    GstFormat fmt = GST_FORMAT_TIME;
    if (gst_element_query_position(m_pPlayBin, fmt, &elapsed))
      time = elapsed/1000000;
  }
  return time;
}

int64_t CGstPlayer::GetTotalTime()
{
  int64_t totaltime = 0;
  if (m_pPlayBin) {
    gint64 duration = 0;
    GstFormat fmt = GST_FORMAT_TIME;
    if (gst_element_query_duration(m_pPlayBin, fmt, &duration))
      totaltime = duration/1000000;
  }
  return totaltime;
}

bool CGstPlayer::GetStreamDetails(CStreamDetails &details)
{
  //TODO: need to implement
  return false;
}

void CGstPlayer::ToFFRW(int iSpeed)
{
  gint64 elapsed = 0;
  GstFormat fmt = GST_FORMAT_TIME;
  CLog::Log(LOGNOTICE, "---[%s]-%d--", __FUNCTION__, iSpeed);
  m_speed = iSpeed;

  if (m_pPlayBin){
    if(gst_element_query_position(m_pPlayBin, fmt, &elapsed)) {
      SetPlaybackRate(iSpeed, elapsed);
      m_callback.OnPlayBackSpeedChanged(iSpeed);
    }
  }
};

void CGstPlayer::Process()
{
  CLog::Log(LOGNOTICE, "GstPlayer thread start %p", m_pPlayBin);
  GstMessage *msg;
  // GstState old, current, pending;
  bool eos = false;

  if (m_pPlayBin) {
    CURL url = m_item.GetURL();
    std::string uri = ParseAndCorrectUrl(url);
    CLog::Log(LOGNOTICE, "Play uri %s", uri.c_str());
    g_object_set(G_OBJECT(m_pPlayBin), "uri", uri.c_str(), NULL);

    if (!SetAndWaitPlaybinState(GST_STATE_PAUSED, 60) || (m_cancelled)){
      m_bStop = true;
      m_ready.Set();
      goto finish;
    }

    LoadMediaInfo();

    if (!SetAndWaitPlaybinState(GST_STATE_PLAYING, 10)){
      m_bStop = true;
      m_ready.Set();
      goto finish;
    }

    g_renderManager.SetViewMode(CMediaSettings::Get().GetCurrentVideoSettings().m_ViewMode);
    
  }

  m_ready.Set();

  while (!m_bStop && !eos)
  {
    msg = gst_bus_timed_pop(m_pBus, 1000000000);
    if(msg) {
      if( GST_MESSAGE_SRC(msg) == GST_OBJECT(m_pPlayBin) ) { //playbin message
        switch(msg->type) {
          case GST_MESSAGE_EOS:
            {
              CLog::Log(LOGNOTICE, "Media EOS");
              eos = true;
              break;
            }
          case GST_MESSAGE_ERROR:
            {
              CLog::Log(LOGERROR, "Media Error");
              eos = true;
              break;
            }

          case GST_MESSAGE_STATE_CHANGED:
            {
              break;
            }

          default:
            break;
        }
      }
      else {
        //TODO: Now we using direct play changes instead of msg, maybe need to check which approach is better
        if( msg->type == GST_MESSAGE_APPLICATION && GST_MESSAGE_SRC(msg) == NULL) {
          // const GstStructure *s = gst_message_get_structure(msg);
          // const gchar *name = gst_structure_get_name(s);
          // if(!g_strcmp0(name, "pause")) {
          //   // if(/*!m_buffering &&*/ !m_paused)
          //     gst_element_set_state(m_pPlayBin, GST_STATE_PLAYING);
          //   // else
          //   //   gst_element_set_state(m_pPlayBin, GST_STATE_PAUSED);
          // }
          // else if(!g_strcmp0(name, "quit")) {
          //   //will quit by m_quit_msg
          // }
        }
        else if(msg->type == GST_MESSAGE_BUFFERING)
        {
          gst_message_parse_buffering(msg, &m_cache_level);
          // printf("Buffering level =%d\n", m_cache_level);
          // if(m_cache_level == 0) {
          //   // m_buffering = true;
          //   gst_element_set_state(m_pPlayBin, GST_STATE_PAUSED);
          // }
          // else if(m_cache_level >= 100) {
          //   // m_buffering = false;
          //   if(!m_paused) {
          //     gst_element_set_state(m_pPlayBin, GST_STATE_PLAYING);
          //   }
          // }
        }
      }
      gst_message_unref(msg);
    }
  }
finish:  
  printf("Player stop begin\n");
  CLog::Log(LOGNOTICE, "Player stop begin");
  gst_bus_set_flushing(m_pBus, TRUE);

  if (!SetAndWaitPlaybinState(GST_STATE_NULL, 30)) {
    CLog::Log(LOGERROR, "Stop player failed");
  }
  if (eos)
    m_callback.OnPlayBackEnded();
  CLog::Log(LOGNOTICE, "Player thread end");
}

bool CGstPlayer::InitializeGst()
{
  if (gst_is_initialized()) 
    return true;
  //Need pass args?
  return gst_init_check(0, 0, NULL);
}

bool CGstPlayer::CreatePlayBin()
{
  GstElement *videosink = NULL;
  m_pPlayBin = CGstPlayBinManager::GetInstance().GetPlayBin();

  if (NULL == m_pPlayBin) {
    CLog::Log(LOGERROR, "%s(): Failed to create playbin!", __FUNCTION__);
    return false;
  }

  // if video-sink is appsink we set callback function
  // TODO: add the same for audio-sink
  g_object_get( G_OBJECT(m_pPlayBin), "video-sink", &videosink, NULL);
  if (videosink && GST_APP_SINK(videosink))
  {
      g_object_set(G_OBJECT(videosink), "emit-signals", TRUE, "sync", TRUE, NULL);
      videosink_handler = g_signal_connect(videosink, "new-sample", G_CALLBACK(OnDecodedBuffer), this);
  }

  m_pBus = gst_pipeline_get_bus(GST_PIPELINE(m_pPlayBin));
  if( NULL == m_pBus )
  {
    CLog::Log(LOGERROR, "%s(): Failed in gst_pipeline_get_bus()!", __FUNCTION__);
    return false;
  }
  printf("PlayBin created. FUNCTION: %s\n", __FUNCTION__);
  return true;
}

bool CGstPlayer::DestroyPlayBin()
{
  printf("FUNCTION: %s\n", __FUNCTION__);

  //remove signal from apssink
  if (videosink_handler) {
    GstElement *videosink = NULL;
    g_object_get( G_OBJECT(m_pPlayBin), "video-sink", &videosink, NULL);
    if (videosink)
      g_signal_handler_disconnect(videosink, videosink_handler);
  }
  // gst_object_unref(m_pPlayBin);
  CGstPlayBinManager::GetInstance().ReleasePlayBin(m_pPlayBin);
  m_pPlayBin = NULL;
  return true;
}

GstFlowReturn CGstPlayer::OnDecodedBuffer(GstAppSink *appsink, void *data)
{
  CGstPlayer *decoder = (CGstPlayer *)data;

  GstSample *sample = gst_app_sink_pull_sample (appsink);

  if (sample)
    decoder->OutputPicture(sample);

  return GST_FLOW_OK;
}

int CGstPlayer::OutputPicture(GstSample *sample)
{
  /* picture buffer is not allowed to be modified in this call */
  DVDVideoPicture picture;
  DVDVideoPicture* pPicture = &picture;

  GstCaps  *caps;
  GstBuffer *buffer;
  GstMapInfo map;

  int result = 0;
  gint flags = 0;
  double config_framerate = 30.0;

  memset(pPicture, 0, sizeof(DVDVideoPicture));

// TODO: check HAS_VIDEO_PLAYBACK define
#ifdef HAS_VIDEO_PLAYBACK

  buffer = gst_sample_get_buffer (sample);
  if (!buffer)
    return result;

  caps = gst_sample_get_caps (sample);

  if (caps) {
    gint width, height;
    const gchar* format;
    GstStructure * structure = gst_caps_get_structure (caps, 0);
    // GstVideoCrop * crop = gst_buffer_get_video_crop (gstbuffer);

    format = gst_structure_get_string (structure, "format");
    // printf("format: %s\n", format);

    if (structure == NULL ||
        !gst_structure_get_int (structure, "width", &width) ||
        !gst_structure_get_int (structure, "height", &height)){
      
      CLog::Log(LOGERROR, "Unsupport output format");
      return -1;
    }

    // printf("format: %s, width: %d, height: %d\n", format, width, height);

    pPicture->iWidth = width;
    pPicture->iHeight = height;

    // if (crop) {
    //   pPicture->iDisplayWidth  = gst_video_crop_width (crop);
    //   pPicture->iDisplayHeight = gst_video_crop_height (crop);
    //   pPicture->iDisplayX = gst_video_crop_left (crop);
    //   pPicture->iDisplayY = gst_video_crop_top (crop);
    // } else {
      pPicture->iDisplayWidth  = pPicture->iWidth;
      pPicture->iDisplayHeight = pPicture->iHeight;
    // }

    /* try various ways to create an eglImg from the decoded buffer: */
    // if (has_ti_raw_video) {
    //   pPicture->eglImageHandle =
    //       new TIRawVideoEGLImageHandle(gstbuffer, width, height, format);
    // }
    // gst_buffer_map (buffer, &map, GST_MAP_READ);
    // ptr = (uint32_t*)map.data;
    /*if (pPicture->eglImageHandle) {
      pPicture->format = DVDVideoPicture::FMT_EGLIMG;
      flags = CONF_FLAGS_FORMAT_EGLIMG;
    } else */ if (g_ascii_strcasecmp (format, "NV12") == 0) {
      gst_buffer_map (buffer, &map, GST_MAP_READ);
      pPicture->data[0] = map.data;
      pPicture->data[1] = map.data + m_output.width*m_output.height;
      pPicture->iLineSize[0] = pPicture->iWidth;
      pPicture->iLineSize[1] = pPicture->iWidth;
      pPicture->format = RENDER_FMT_NV12;
      // flags = CONF_FLAGS_FORMAT_NV12;
      gst_buffer_unmap (buffer, &map);
    } else if (g_ascii_strcasecmp (format, "I420") == 0) {
      gst_buffer_map (buffer, &map, GST_MAP_READ);
      pPicture->data[0] = map.data;
      pPicture->data[1] = map.data + m_output.width*m_output.height;
      pPicture->data[2] = map.data + m_output.width*m_output.height*5/4;
      pPicture->iLineSize[0] = pPicture->iWidth;
      pPicture->iLineSize[1] = pPicture->iWidth/2;
      pPicture->iLineSize[2] = pPicture->iWidth/2;
      pPicture->format = RENDER_FMT_YUV420P;
      // flags = CONF_FLAGS_FORMAT_YV12;
      // flags = 1938;
      //TODO: optimazi buffer operation
      gst_buffer_unmap (buffer, &map);
    } else {
      CLog::Log(LOGERROR, "Unsupport output format aaaa");
      return -1;
    }
    
  }else{
    return -1;
  }


  if ((m_output.width != pPicture->iWidth) || (m_output.height != pPicture->iHeight)){
    if(!g_renderManager.Configure(pPicture->iWidth,
                                 pPicture->iHeight,
                                 pPicture->iDisplayWidth,
                                 pPicture->iDisplayHeight,
                                 config_framerate,
                                 flags,
                                 pPicture->format,
                                 pPicture->extended_format,
                                 0,
                                 0))
    {
      CLog::Log(LOGNOTICE, "Failed to configure renderer");
      return -1;
    }

    printf("%s - change configuration.\n"
      "%dx%d. \n"
      "Display %dx%d. \n"
      " framerate: %4.2f\n."
      "flags %d\n"
      "format %d\n",
      __FUNCTION__,
      pPicture->iWidth, pPicture->iHeight,
      pPicture->iDisplayWidth, pPicture->iDisplayHeight,
      config_framerate,
      flags,
      pPicture->format);

    m_output.width=pPicture->iWidth;
    m_output.height=pPicture->iHeight;
  }

  // int buf = g_renderManager.WaitForBuffer(m_bStop);
  // if (buf < 0)
  //   printf("wow\n");
  // printf("buffs = %d\n", buf);
  
  int index = g_renderManager.AddVideoPicture(*pPicture);
#if 0
  // video device might not be done yet
  while (index < 0 && !CThread::m_bStop &&
         CDVDClock::GetAbsoluteClock(false) < iCurrentClock + iSleepTime + DVD_MSEC_TO_TIME(500) )
  {
    Sleep(1);
    index = g_renderManager.AddVideoPicture(*pPicture);
  }
#endif
  if (index < 0)
    return -1;

  g_renderManager.FlipPage(CThread::m_bStop, 0LL, 0, -1, FS_NONE);

  // if (pPicture->eglImageHandle)
  //   pPicture->eglImageHandle->UnRef();

  return result;

#endif
}

std::string CGstPlayer::ParseAndCorrectUrl(CURL &url)
{
  std::string strProtocol = url.GetTranslatedProtocol();
  std::string ret;
  url.SetProtocol(strProtocol);

  // ResetUrlInfo();
  if(url.IsLocal()) { //local file
    std::string path = url.GetFileName();
    gchar *fname = NULL;
    if(url.IsFullPath(path))
      fname = g_strdup(path.c_str());
    else {
      gchar *pwd = g_get_current_dir();
      fname = g_strdup_printf("%s/%s", pwd, path.c_str());
      g_free(pwd);
    }
    url.SetFileName(std::string(fname));
    url.SetProtocol("file");
    g_free(fname);
  }
  else if( !strProtocol.compare("http")
      ||  !strProtocol.compare("https"))
  {
    
    // replace invalid spaces
    std::string strFileName = url.GetFileName();
    StringUtils::Replace(strFileName," ", "%20");
    url.SetFileName(strFileName);

    //TODO: for youtube need to remove User-Agent string (?)
    std::vector<std::string> list = StringUtils::Split(url.Get(), '|');
    url.Parse(list.front());

    // // get username and password
    // m_username = url.GetUserName();
    // m_password = url.GetPassWord();

  }

  // if (m_username.length() > 0 && m_password.length() > 0)
  //   ret = url.GetWithoutUserDetails();
  // else
    ret = url.Get();
  printf("++++++++++URL to play: %s\n", ret.c_str());
  return ret;
}

bool CGstPlayer::SetAndWaitPlaybinState(GstState newstate, int timeout)
{
  GstStateChangeReturn ret;

  if (m_pPlayBin){
    ret = gst_element_set_state(m_pPlayBin, newstate);
    if (ret==GST_STATE_CHANGE_ASYNC){
      GstState current, pending;
      do{
      ret =  gst_element_get_state(m_pPlayBin, &current, &pending, GST_SECOND);
        }while((m_cancelled==false)&&(ret==GST_STATE_CHANGE_ASYNC) && (timeout-->=0));
    }

    if ((ret == GST_STATE_CHANGE_FAILURE) || (ret == GST_STATE_CHANGE_ASYNC)){
      return false;
    }else{
      return true;
    }
  }
  return false;
  
}

bool CGstPlayer::LoadMediaInfo()
{
  if (m_pPlayBin){
    int i;
    GstQuery *query;

    g_object_get( G_OBJECT(m_pPlayBin), "n-video", &m_mediainfo.video_num, NULL);
    g_object_get( G_OBJECT(m_pPlayBin), "n-audio", &m_mediainfo.audio_num, NULL);

    g_object_get( G_OBJECT(m_pPlayBin), "current-audio", &m_audio_current, NULL);
    if (m_audio_current < 0)
      m_audio_current=0;

    g_object_get( G_OBJECT(m_pPlayBin), "current-video", &m_video_current, NULL);
    if (m_video_current < 0)
      m_video_current=0;

    query = gst_query_new_seeking (GST_FORMAT_TIME);
    if(gst_element_query (m_pPlayBin, query))
      gst_query_parse_seeking (query, NULL, &m_mediainfo.seekable, NULL, NULL);
    else
      m_mediainfo.seekable=false;
    gst_query_unref (query);

    if (m_mediainfo.audio_num){
      m_mediainfo.audio_info = (MediaAudioInfo*)g_malloc(sizeof(MediaAudioInfo)*m_mediainfo.audio_num);
      memset(m_mediainfo.audio_info, 0, (sizeof(MediaAudioInfo)*m_mediainfo.audio_num));
    }

    if (m_mediainfo.video_num){
      m_mediainfo.video_info = (MediaVideoInfo*)g_malloc(sizeof(MediaVideoInfo)*m_mediainfo.video_num);
      memset(m_mediainfo.video_info, 0, (sizeof(MediaVideoInfo)*m_mediainfo.video_num));
    }

    for (i=0; i<m_mediainfo.audio_num; i++)
    {
      GstTagList *tags = NULL;

      g_signal_emit_by_name (G_OBJECT (m_pPlayBin), "get-audio-tags", i, &tags);

      if (tags) {
        MediaAudioInfo * ainfo = &m_mediainfo.audio_info[i];

        LOAD_TAG(ainfo->lang, tags, GST_TAG_LANGUAGE_CODE);
        LOAD_TAG(ainfo->codec, tags, GST_TAG_CODEC);
        if (ainfo->codec==NULL){
          LOAD_TAG(ainfo->codec, tags, GST_TAG_AUDIO_CODEC);
        }

        gst_tag_list_get_uint(tags, GST_TAG_BITRATE, &ainfo->bitrate);

        LOAD_TAG(m_mediainfo.container_format, tags, GST_TAG_CONTAINER_FORMAT);
        LOAD_TAG(m_mediainfo.title, tags, GST_TAG_TITLE);
        LOAD_TAG(m_mediainfo.artist, tags, GST_TAG_ARTIST);
        LOAD_TAG(m_mediainfo.description, tags, GST_TAG_DESCRIPTION);
        LOAD_TAG(m_mediainfo.album, tags, GST_TAG_ALBUM);
        LOAD_TAG(m_mediainfo.genre, tags, GST_TAG_GENRE);

        gst_tag_list_free (tags);
      }
    }

    for (i=0;i<m_mediainfo.video_num;i++)
    {
      GstTagList *tags = NULL;

      g_signal_emit_by_name (G_OBJECT (m_pPlayBin), "get-video-tags", i, &tags);

      if (tags) {
        MediaVideoInfo * vinfo = &m_mediainfo.video_info[i];

        LOAD_TAG(vinfo->codec, tags, GST_TAG_CODEC);
        if (vinfo->codec==NULL){
          LOAD_TAG(vinfo->codec, tags, GST_TAG_VIDEO_CODEC);
        }
        gst_tag_list_get_uint(tags, GST_TAG_BITRATE, &vinfo->bitrate);

        LOAD_TAG(m_mediainfo.container_format, tags, GST_TAG_CONTAINER_FORMAT);
        LOAD_TAG(m_mediainfo.title, tags, GST_TAG_TITLE);
        LOAD_TAG(m_mediainfo.artist, tags, GST_TAG_ARTIST);
        LOAD_TAG(m_mediainfo.description, tags, GST_TAG_DESCRIPTION);
        LOAD_TAG(m_mediainfo.album, tags, GST_TAG_ALBUM);
        LOAD_TAG(m_mediainfo.genre, tags, GST_TAG_GENRE);

        gst_tag_list_free (tags);
      }
    }

    m_mediainfo.loaded = true;
    return true;
  } 
  return false;
}

void CGstPlayer::CleanMediaInfo()
{
  int i;

  if (m_mediainfo.container_format){
    g_free(m_mediainfo.container_format);
    m_mediainfo.container_format = NULL;
  }
  if (m_mediainfo.genre){
    g_free(m_mediainfo.genre);
    m_mediainfo.genre = NULL;
  }
  if (m_mediainfo.title){
    g_free(m_mediainfo.title);
    m_mediainfo.title = NULL;
  }
  if (m_mediainfo.artist){
    g_free(m_mediainfo.artist);
    m_mediainfo.artist = NULL;
  }
  if (m_mediainfo.description){
    g_free(m_mediainfo.description);
    m_mediainfo.description = NULL;
  }
  if (m_mediainfo.album){
    g_free(m_mediainfo.album);
    m_mediainfo.album = NULL;
  }

  if (m_mediainfo.audio_info){
    for (i=0;i<m_mediainfo.audio_num;i++){
      MediaAudioInfo * ainfo = &m_mediainfo.audio_info[i];

      if (ainfo->lang){
    g_free(ainfo->lang);
    ainfo->lang = NULL;
  }
      if (ainfo->codec){
    g_free(ainfo->codec);
    ainfo->codec = NULL;
  }
      
    }
    g_free(m_mediainfo.audio_info);
    m_mediainfo.audio_info = NULL;
  }

  if (m_mediainfo.video_info){
    for (i=0;i<m_mediainfo.video_num;i++){
      MediaVideoInfo * vinfo = &m_mediainfo.video_info[i];
      if (vinfo->codec){
    g_free(vinfo->codec);
    vinfo->codec = NULL;
  }
    }
    g_free(m_mediainfo.video_info);
    m_mediainfo.video_info = NULL;
  }
}

void CGstPlayer::SetPlaybackRate(int iSpeed, gint64 pos)
{
  GstEvent *event;
  if(iSpeed > 0)
    event = gst_event_new_seek(iSpeed, GST_FORMAT_TIME,
        (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
        GST_SEEK_TYPE_SET, pos,
        GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
  else
    event = gst_event_new_seek(iSpeed, GST_FORMAT_TIME,
        (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
        GST_SEEK_TYPE_SET, 0,
        GST_SEEK_TYPE_SET, pos);
  gst_element_send_event(m_pPlayBin, event);
}

void CGstPlayer::SetSinkRenderDelay(GstElement *ele, guint64 renderDelay)
{
  if (GST_IS_BIN(ele)){
    GstIterator *elem_it = NULL;
    gboolean done = FALSE;
    elem_it = gst_bin_iterate_sinks (GST_BIN (ele));
    while (!done) {
      GValue value = { 0, };

      switch (gst_iterator_next (elem_it, &value)) {
        case GST_ITERATOR_OK:
          GstElement *element;
          element = GST_ELEMENT (g_value_get_object (&value));
          if (element ) {
            g_object_set( G_OBJECT(element), "render-delay", renderDelay, NULL);
          }
          g_value_unset (&value);
          break;
        case GST_ITERATOR_RESYNC:
          gst_iterator_resync (elem_it);
          break;
        case GST_ITERATOR_ERROR:
          done = TRUE;
          break;
        case GST_ITERATOR_DONE:
          done = TRUE;
          break;
      }
    }
    gst_iterator_free (elem_it);
  }else{
    g_object_set( G_OBJECT(ele), "render-delay", renderDelay, NULL);
  }
}