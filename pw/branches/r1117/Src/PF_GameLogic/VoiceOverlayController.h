#ifndef VOICEOVERLAYCONTROLLER_H_INCLUDED
#define VOICEOVERLAYCONTROLLER_H_INCLUDED

#include "UI/FlashInterface.h"
#include "UI/FSCommandListner.h"
#include "UI/FlashContainer2.h"
#include "PFTraining.h"

namespace NGameX
{

class ChatUiController;

// Voice chat overlay in the combat UI. Voice itself (WebRTC) lives in the launcher;
// the game polls it over a local UDP socket (127.0.0.1:34981, see launcher
// content/modules/voiceGameBridge.js), passes the call state to main.swf
// (MainInterface.SetVoiceOverlay*) and forwards overlay clicks and the push-to-talk
// key back. Without a launcher answer the overlay just stays hidden.
// The overlay lets clicks through to the game; it takes the mouse (mute, volume,
// dragging) only while the voice_overlay_unlock bind is held (SHIFT by default,
// rebindable in the in-game shortcuts menu and in the launcher).
class VoiceOverlayController: public UI::FlashInterface, public UI::IFSCommandListner
{
  NI_DECLARE_REFCOUNT_CLASS_2( VoiceOverlayController, UI::FlashInterface, UI::IFSCommandListner )

public:
  // pollUnlockKey: read the voice_overlay_unlock keys directly instead of waiting for
  // SetUnlocked() - for screens where the adventure_screen bind section is not active
  // (the loading screen).
  VoiceOverlayController( UI::FlashContainer2 * flashWnd, const char* className, bool pollUnlockKey = false );
  ~VoiceOverlayController();

  void Update( float timeDelta );
  void SetUnlocked( bool unlocked );
  // When the voice first appears on this screen, a hint with the player's own unlock key goes
  // to this chat as a local system message (seen only by this player).
  void SetChat( ChatUiController * chat );
  // Smart cast switched on the action bar: the launcher saves it into the build (back end).
  void NotifySmartCast( const char * talentId, bool on );
  // Training: a hero chosen in the battle - the launcher answers with the player's build of it (back end
  // build.trainingBuild). The answer comes later; TakeTrainingBuild gives it once: true when the launcher answered
  // this request, build.talents are all empty when the launcher has no build (the build of the data then).
  bool IsLauncherAnswering() const;
  void RequestTrainingBuild( int requestId, const char * heroId );
  bool TakeTrainingBuild( int requestId, Training::WebBuild & build );
  // Consumables of the action bar chosen in the launcher (back end build.battleConsumables): asked again
  // until the launcher answers. Read only - the battle never saves them. GetConsumableBar: false until then,
  // masks - a bit per consumable (CONSUMABLE_BAR_KINDS) for each of the 24 buttons.
  enum { CONSUMABLE_BAR_SLOTS = 24 };
  void RequestConsumableBar();
  bool GetConsumableBar( int masks[CONSUMABLE_BAR_SLOTS] ) const;
  // a line of Bin\voice_overlay.log (rare events only)
  static void Log( const char * format, ... );

  // Must run before AdventureScreen registers its handlers: a handler resolves its
  // command by name once, at registration, and never sees a command bound later.
  static void EnsureDefaultUnlockBind();

private:
  struct Peer
  {
    int id;
    int flags;
    int volume;
    wstring name;

    bool operator == ( const Peer & other ) const { return id == other.id && flags == other.flags && volume == other.volume && name == other.name; }
  };

  struct State
  {
    bool enabled;
    bool radioMode;
    int micState;
    bool meSpeaking;
    wstring myName;
    float posX;
    float posY;
    nstl::vector<Peer> peers;
    nstl::vector<int> pttKeys;

    State() : enabled(false), radioMode(false), micState(0), meSpeaking(false), posX(-1.f), posY(-1.f) {}
  };

  virtual void OnFSCommand( UI::FlashContainer2* _wnd, const char* listenerID, const char* args, const wchar_t * argsW );

  bool OpenSocket();
  void CloseSocket();
  void Send( const char * text );
  void ReceiveAll();
  bool ParseState( const char * data, int size, State & state );
  bool ParseTrainingBuild( const char * data, int size );
  bool ParseConsumableBar( const char * data, int size );
  void UpdateConsumableBarRequest( float timeDelta );
  void ApplyState( const State & state );
  void HideOverlay();
  void UpdatePushToTalk();
  bool IsPushToTalkHeld() const;
  static bool IsUnlockBindHeld();
  static wstring GetUnlockKeyText();
  void ShowUnlockHint();

  UINT_PTR socketHandle;
  bool socketFailed;
  bool winsockStarted;
  float pollTimer;
  float silenceTime;
  bool shown;
  bool pttHeld;
  bool unlocked;
  bool pollUnlockKey;
  Weak<ChatUiController> chat;
  bool hintShown;
  State current;
  bool hasCurrent;
  bool launcherHeard;  // the launcher answered a poll at least once
  // answers of the launcher by the request (several heroes may be added at once), until taken
  nstl::map<int, Training::WebBuild> trainingBuilds;
  bool consumableBarWanted;
  bool consumableBarKnown;
  int consumableBarTries;
  float consumableBarTimer;
  int consumableBar[CONSUMABLE_BAR_SLOTS];
};

}

#endif //VOICEOVERLAYCONTROLLER_H_INCLUDED
