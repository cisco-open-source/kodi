#pragma once

#include <gst/gst.h>
#include <list>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <stdio.h>
#include <stdint.h>

class CGstPlayBinManager
{

  public:
    // Need thread safe impl
    static CGstPlayBinManager& GetInstance(); 
    virtual ~CGstPlayBinManager(); 

    GstElement* GetPlayBin();
    void ReleasePlayBin(GstElement*);

  private:
    CGstPlayBinManager();
    CGstPlayBinManager( const CGstPlayBinManager& );  
    CGstPlayBinManager& operator=( CGstPlayBinManager& );

    enum m_playbinState {
        PLAYBIN_STATE_BUSY,
        PLAYBIN_STATE_IDLE
    };

    enum m_playbinType {
        PLAYBIN_TYPE_HARDWARE,
        PLAYBIN_TYPE_SOFTWARE
    };

    struct SPlayer {
        SPlayer() { Clear(); }
        void Clear()
        {
          playBin       = NULL;
          state         = PLAYBIN_STATE_IDLE;
          type          = PLAYBIN_TYPE_HARDWARE;
          keepPlayBin   = false;

        }
        GstElement*     playBin;
        m_playbinState  state;
        m_playbinType   type;
        bool            keepPlayBin; //if true no need to destroy pipeline after playback
    };

    struct SPlayerDescription {
        m_playbinType   type;
        std::string     playBin;
        std::string     videoSink;
        std::string     audioSink;
        uint32_t        flags;
        bool            keepPlayBin;
    };

    typedef boost::shared_ptr<SPlayer> SPlayerPtr;
    
    SPlayerPtr getAvaliblePlayer();
    SPlayerPtr createPlayer();
    GstElement* getVideoSink(const SPlayerDescription& playerDescr);
    GstElement* getAudioSink(const SPlayerDescription& playerDescr);

    void InitPlayerDescriptions();
    SPlayerDescription GetDefaultPlayerDescription();

    //may need optimization
    std::list<SPlayerPtr> m_playerList;
    std::vector<SPlayerDescription> m_playerDescriptions;
};