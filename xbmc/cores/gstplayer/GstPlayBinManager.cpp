#include "GstPlayBinManager.h"

#include "utils/log.h"
#include "utils/XMLUtils.h"
#include "filesystem/File.h"

const char DEFAULT_PLAYBIN[]    = "playbin";
const char DEFAULT_VIDEO_SINK[] = "appsink";
const char DEFAULT_AUDIO_SINK[] = "";

const char  GST_PLAYERS_XML[] = "gstplayers.xml";

const char PLAYERS_XML[]      = "players";
const char PLAYER_XML[]       = "player";
const char TYPE_XML[]         = "type";
const char PLAYBIN_XML[]      = "playbin";
const char VIDEO_SINK_XML[]   = "video_sink";
const char AUDIO_SINK_XML[]   = "audio_sink";
const char FLAGS_XML[]        = "flags";
const char KEEP_PLAYBIN_XML[] = "keep_playbin";

CGstPlayBinManager::CGstPlayBinManager()
{
  //load player description from xml
  InitPlayerDescriptions();
}

CGstPlayBinManager::~CGstPlayBinManager()
{
  while(!m_playerList.empty()) {
    gst_object_unref(m_playerList.front()->playBin);
    m_playerList.pop_front();
  }
}

CGstPlayBinManager& CGstPlayBinManager::GetInstance()
{
  static CGstPlayBinManager s;
  return s;
}

GstElement* CGstPlayBinManager::GetPlayBin()
{
  SPlayerPtr player;
  player = getAvaliblePlayer();
  if (!player) 
    player = createPlayer();

  if (player) {
    player->state = PLAYBIN_STATE_BUSY;
    return player->playBin;
  }
  else
    return NULL;

}

void CGstPlayBinManager::ReleasePlayBin(GstElement* pPlaybin)
{
  std::list<SPlayerPtr>::iterator it;
  for (it = m_playerList.begin(); it != m_playerList.end(); ++it) {
    SPlayerPtr player = *it;
    if (player->playBin == pPlaybin)
    {
      if (player->keepPlayBin)
        player->state = PLAYBIN_STATE_IDLE;
      else {
        gst_object_unref(player->playBin);
        m_playerList.erase(it);
      }
      break;
    }
  }
}

CGstPlayBinManager::SPlayerPtr CGstPlayBinManager::getAvaliblePlayer()
{
  SPlayerPtr player;
  if (m_playerList.empty()) {
    return player;
  }
  else {
    // return first created not busy player;
    // TODO: need implementing more smart selection: e.g selection by type
    std::list<SPlayerPtr>::iterator it;
    for (it = m_playerList.begin(); it != m_playerList.end(); ++it) {
      if ((*it)->state == PLAYBIN_STATE_IDLE) {
        player = *it;
        break;
      }
    }
    return player;   
  }
}

CGstPlayBinManager::SPlayerPtr CGstPlayBinManager::createPlayer()
{
  SPlayerPtr player(new SPlayer());

  // TODO: select implementation from description, for now take first
  SPlayerDescription playerDescr = m_playerDescriptions.front();

  player->playBin = gst_element_factory_make(playerDescr.playBin.c_str(), "playbin");

  if(!player->playBin)
  {
    CLog::Log(LOGERROR, "%s: gst_element_factory_make %s failed", __FUNCTION__, playerDescr.playBin.c_str());
    player.reset();
    return player;
  }

  GstElement* videosink = getVideoSink(playerDescr);
  GstElement* audiosink = getAudioSink(playerDescr);

  if (videosink)
    g_object_set(G_OBJECT(player->playBin), "video-sink", videosink, NULL);

  if (audiosink)
    g_object_set(G_OBJECT(player->playBin), "audio-sink", audiosink, NULL);

  if (playerDescr.flags)
    g_object_set(G_OBJECT(player->playBin), "flags", playerDescr.flags, NULL);

  player->keepPlayBin = playerDescr.keepPlayBin;

  m_playerList.push_back(player);

  return player;

}

GstElement* CGstPlayBinManager::getVideoSink(const SPlayerDescription& playerDescr)
{
  if (playerDescr.videoSink.empty())
    return NULL;

  GstElement* videosink = gst_element_factory_make(playerDescr.videoSink.c_str(), "videosink");
  if(!videosink)
  {
    CLog::Log(LOGERROR, "%s: gst_element_factory_make %s failed", __FUNCTION__, playerDescr.videoSink.c_str());
    return NULL;
  }

  return videosink;
}

GstElement* CGstPlayBinManager::getAudioSink(const SPlayerDescription& playerDescr)
{
  if (playerDescr.audioSink.empty())
    return NULL;

  GstElement* audiosink = gst_element_factory_make(playerDescr.audioSink.c_str(), "audiosink");
  if(!audiosink)
  {
    CLog::Log(LOGERROR, "%s: gst_element_factory_make %s failed", __FUNCTION__, playerDescr.audioSink.c_str());
    return NULL;
  }

  return audiosink;

}

void CGstPlayBinManager::InitPlayerDescriptions()
{
  SPlayerDescription playerDescr = GetDefaultPlayerDescription();
  std::string file = "special://xbmc/system/players/gstplayer/" + std::string(GST_PLAYERS_XML);
  CLog::Log(LOGNOTICE, "%s: Loading gst players settings from %s.",  __FUNCTION__, file.c_str());

  if (!XFILE::CFile::Exists(file))
  { 
    CLog::Log(LOGNOTICE, "%s: %s does not exist. Skipping.", __FUNCTION__, file.c_str());
    m_playerDescriptions.push_back(playerDescr);
    return;
  }

  CXBMCTinyXML playerCoreFactoryXML;
  if (!playerCoreFactoryXML.LoadFile(file))
  {
    CLog::Log(LOGERROR, "%s: Error loading %s, Line %d (%s)",  __FUNCTION__, file.c_str(), playerCoreFactoryXML.ErrorRow(), playerCoreFactoryXML.ErrorDesc());
    m_playerDescriptions.push_back(playerDescr);
    return;
  }

  TiXmlElement *pConfig = playerCoreFactoryXML.RootElement();
  if (pConfig == NULL)
  {
      CLog::Log(LOGERROR, "%s: Error loading %s, Bad structure", __FUNCTION__, file.c_str());
      m_playerDescriptions.push_back(playerDescr);
      return;
  }

  TiXmlElement* pPlayer = pConfig->FirstChildElement(PLAYER_XML);
  while (pPlayer)
  {
    playerDescr = GetDefaultPlayerDescription();
    std::string name = XMLUtils::GetAttribute(pPlayer, "name");

    // TODO: getting player type
    // TiXmlElement* pType = pPlayer->FirstChildElement(TYPE_XML);
    // if (pType) {
    //   // playerDescr.type = 
    // }

    XMLUtils::GetString(pPlayer, PLAYBIN_XML, playerDescr.playBin);
    XMLUtils::GetString(pPlayer, VIDEO_SINK_XML, playerDescr.videoSink);
    XMLUtils::GetString(pPlayer, AUDIO_SINK_XML, playerDescr.audioSink);

    XMLUtils::GetHex(pPlayer, FLAGS_XML, playerDescr.flags);
    XMLUtils::GetBoolean(pPlayer, KEEP_PLAYBIN_XML, playerDescr.keepPlayBin);

    CLog::Log(LOGNOTICE, "%s: Player name = %s; playbin = %s; audiosink = %s, videosink = %s, flags = 0x%x, keep = %d\n ",
            __FUNCTION__, name.c_str(), playerDescr.playBin.c_str(), playerDescr.audioSink.c_str(),
             playerDescr.videoSink.c_str(), playerDescr.flags, playerDescr.keepPlayBin);

    pPlayer = pPlayer->NextSiblingElement(PLAYER_XML);

    m_playerDescriptions.push_back(playerDescr);
  }

  if (m_playerDescriptions.empty())
    m_playerDescriptions.push_back(playerDescr);
  
  CLog::Log(LOGNOTICE, "%s: Loaded gst players configuration. Got %d players", __FUNCTION__ ,m_playerDescriptions.size());

}

CGstPlayBinManager::SPlayerDescription CGstPlayBinManager::GetDefaultPlayerDescription()
{
  SPlayerDescription playerDescr;

  playerDescr.type = PLAYBIN_TYPE_HARDWARE;
  playerDescr.playBin = DEFAULT_PLAYBIN;
  playerDescr.videoSink = DEFAULT_VIDEO_SINK;
  playerDescr.audioSink = DEFAULT_AUDIO_SINK;
  playerDescr.flags = 0;
  playerDescr.keepPlayBin = false;

  return playerDescr;
}