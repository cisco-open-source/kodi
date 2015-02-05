#pragma once
#include "cores/IPlayer.h"
#include "threads/Thread.h"
#include "FileItem.h"
#include "URL.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

typedef struct{
   gchar * codec;
   gchar * lang;
   guint bitrate;
} MediaAudioInfo;

typedef struct {
   gchar * codec;
   guint bitrate;
} MediaVideoInfo;

typedef struct{
   bool loaded;
   gint video_num;
   gint audio_num;
   gint sub_num;
   gchar * container_format;
   gchar * title;
   gchar * genre;
   gchar * artist;
   gchar * description;
   gchar * album;
   gboolean seekable;
   MediaAudioInfo * audio_info;
   MediaVideoInfo * video_info;
} MediaInfo;

class CGstPlayer : public IPlayer, public CThread
{
public:
	CGstPlayer(IPlayerCallback& callback);
  virtual ~CGstPlayer();
  virtual bool Initialize(TiXmlElement* pConfig) { return true; };
  virtual bool OpenFile(const CFileItem& file, const CPlayerOptions& options);
  virtual bool QueueNextFile(const CFileItem &file) { return false; }
  virtual void OnNothingToQueueNotify() { }
  virtual bool CloseFile(bool reopen = false);
  virtual bool IsPlaying() const;
  virtual bool CanPause() { return true; };
  virtual void Pause();
  virtual bool IsPaused() const;
  virtual bool HasVideo() const;
  virtual bool HasAudio() const;
  virtual bool IsPassthrough() const { return false;}
  virtual bool CanSeek();
  virtual void Seek(bool bPlus = true, bool bLargeStep = false, bool bChapterOverride = false);
  virtual bool SeekScene(bool bPlus = true) { return false;}
  virtual void SeekPercentage(float fPercent = 0);
  virtual float GetPercentage();
  virtual float GetCachePercentage(){ return 0;}
  virtual void SetMute(bool bOnOff);
  virtual void SetVolume(float volume);
  virtual bool ControlsVolume();
  virtual void SetDynamicRangeCompression(long drc){ }
  virtual void GetAudioInfo( std::string& strAudioInfo);
  virtual void GetVideoInfo( std::string& strVideoInfo);
  virtual void GetGeneralInfo( std::string& strVideoInfo);
  virtual bool CanRecord();
  virtual bool IsRecording() { return false;};
  virtual bool Record(bool bOnOff) { return false;};

  virtual void  SetAVDelay(float fValue = 0.0f);
  virtual float GetAVDelay();

  virtual void SetSubTitleDelay(float fValue = 0.0f){};
  virtual float GetSubTitleDelay()    { return 0.0f; }
  virtual int  GetSubtitleCount()     { return 0; }
  virtual int  GetSubtitle()          { return -1; }
  virtual void GetSubtitleStreamInfo(int index, SPlayerSubtitleStreamInfo &info){ };
  virtual void SetSubtitle(int iStream){ };
  virtual bool GetSubtitleVisible(){  return false;};
  virtual void SetSubtitleVisible(bool bVisible){ };
  virtual int  AddSubtitle(const std::string& strSubPath) { return -1;};

  virtual int  GetAudioStreamCount();
  virtual int  GetAudioStream();
  virtual void SetAudioStream(int iStream);
  virtual void GetAudioStreamInfo(int index, SPlayerAudioStreamInfo &info);

  virtual TextCacheStruct_t* GetTeletextCache() { return NULL; };
  virtual void LoadPage(int p, int sp, unsigned char* buffer) {};

  // virtual int  GetChapterCount()                               { return 0; }
  // virtual int  GetChapter()                                    { return -1; }
  // virtual void GetChapterName(std::string& strChapterName)     { return; }
  // virtual int  SeekChapter(int iChapter)                       { return -1; }
//  virtual bool GetChapterInfo(int chapter, SChapterInfo &info) { return false; }

  virtual float GetActualFPS() { return 0.0f; };
  virtual void SeekTime(int64_t iTime = 0);
  /*!
   \brief current time in milliseconds
   */
  virtual int64_t GetTime();
  /*!
   \brief total time in milliseconds
   */
  virtual int64_t GetTotalTime();
  virtual void GetVideoStreamInfo(SPlayerVideoStreamInfo &info);
  virtual int GetSourceBitrate(){ return 0;}
  virtual bool GetStreamDetails(CStreamDetails &details);
  virtual void ToFFRW(int iSpeed = 0);
  // Skip to next track/item inside the current media (if supported).
  virtual bool SkipNext(){ return false;}

  //Returns true if not playback (paused or stopped beeing filled)
  virtual bool IsCaching() const;
  //Cache filled in Percent
  virtual int GetCacheLevel() const;

  // Menu is DVD/Blueray feature;
  virtual bool IsInMenu() const { return false;};
  virtual bool HasMenu() { return false; };

  virtual bool OnAction(const CAction &action) { /*printf("FUNCTION: %s\n", __FUNCTION__);*/ return false; };

  //returns a state that is needed for resuming from a specific time
  virtual std::string GetPlayerState() { return ""; };
  virtual bool SetPlayerState(const std::string& state) { return false;};
  
  virtual std::string GetPlayingTitle() {/*at the moment DVDPlayer supports only for Teletext*/ return ""; };

  virtual bool SwitchChannel(PVR::CPVRChannel &channel) { return false; }

  // // Note: the following "OMX" methods are deprecated and will be removed in the future
  // // They should be handled by the video renderer, not the player
  // /*!
  //  \brief If the player uses bypass mode, define its rendering capabilities
  //  */
  // virtual void OMXGetRenderFeatures(std::vector<int> &renderFeatures) {};
  // /*!
  //  \brief If the player uses bypass mode, define its deinterlace algorithms
  //  */
  // virtual void OMXGetDeinterlaceMethods(std::vector<int> &deinterlaceMethods) {};
  // /*!
  //  \brief If the player uses bypass mode, define how deinterlace is set
  //  */
  // virtual void OMXGetDeinterlaceModes(std::vector<int> &deinterlaceModes) {};
  // /*!
  //  \brief If the player uses bypass mode, define its scaling capabilities
  //  */
  // virtual void OMXGetScalingMethods(std::vector<int> &scalingMethods) {};
  // /*!
  //  \brief define the audio capabilities of the player (default=all)
  //  */

  // virtual void GetAudioCapabilities(std::vector<int> &audioCaps) { audioCaps.assign(1,IPC_AUD_ALL); };
  // /*!
  //  \brief define the subtitle capabilities of the player
  //  */
  // virtual void GetSubtitleCapabilities(std::vector<int> &subCaps) { subCaps.assign(1,IPC_SUBS_ALL); };

protected:
  virtual void Process();
  virtual bool CreatePlayBin();
  virtual bool DestroyPlayBin();

private:
  //Consider move to CApplication::Create()
  bool InitializeGst();



  std::string ParseAndCorrectUrl(CURL &url);
  bool SetAndWaitPlaybinState(GstState newstate, int timeout);
  bool LoadMediaInfo();
  void CleanMediaInfo();
  void SetPlaybackRate(int iSpeed, gint64 pos);
  void SetSinkRenderDelay(GstElement * ele, guint64 renderDelay);

  static GstFlowReturn OnDecodedBuffer(GstAppSink *sink, void *data);
  int OutputPicture(GstSample * sample);

  void AppendInfoString(std::string& infoStr, gchar* data, const std::string& prefix, const std::string& postfix);


  CFileItem   m_item;
  GstElement *m_pPlayBin;
  GstBus     *m_pBus;

  gulong videosink_handler;

  MediaInfo m_mediainfo;
  int m_audio_current;
  int m_video_current;

  //?
  bool m_cancelled;
  bool m_paused;
  gint m_cache_level;
  bool m_buffering;
  //?
  int m_speed;
  gint64 m_starttime;
  CEvent m_ready;

  struct SOutputConfiguration
  {
    unsigned int width;
    unsigned int height;
    unsigned int dwidth;
    unsigned int dheight;
    unsigned int color_format;
    unsigned int extended_format;
    unsigned int color_matrix : 4;
    unsigned int color_range  : 1;
    unsigned int chroma_position;
    unsigned int color_primaries;
    unsigned int color_transfer;
    double       framerate;
  } m_output; //?

};