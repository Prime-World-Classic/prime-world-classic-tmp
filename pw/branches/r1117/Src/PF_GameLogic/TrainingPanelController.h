#ifndef TRAININGPANELCONTROLLER_H_INCLUDED
#define TRAININGPANELCONTROLLER_H_INCLUDED

#include "UI/FlashInterface.h"
#include "UI/FSCommandListner.h"
#include "UI/FlashContainer2.h"
#include "BindsController.h"
#include "PFTraining.h"

namespace NWorld
{
  class PFWorld;
  class PFBaseUnit;
  class PFBaseHero;
}

namespace NGameX
{

class AdventureScreen;
class VoiceOverlayController;

// Training panel of the combat UI (main.swf, Training/TrainingPanel.as): learn the next talent in order / all talents, refresh,
// training modes (no damage, no cooldowns, no mana), hero and skin change, heroes added on the player's side or
// against it, a target dummy with a damage counter, removal of the selected added hero. The panel collapses to a small
// tab at the left edge of the screen (user var training_panel_collapsed).
// Buttons come back as FSCommand "TrainingPanelCmd" and go to the world as the command CmdTraining (PFTraining.cpp):
// builds with cheats have the panel in any battle, shipping builds only on the training map with one human player.
// The player's new hero chosen in the list comes with the player's build of it: the launcher is asked over the voice
// bridge (launcherBridge), no answer in BUILD_TIMEOUT - the build of the data. Added heroes take the build of the data.
class TrainingPanelController: public UI::FlashInterface, public UI::IFSCommandListner
{
  NI_DECLARE_REFCOUNT_CLASS_2( TrainingPanelController, UI::FlashInterface, UI::IFSCommandListner )

public:
  TrainingPanelController( UI::FlashContainer2 * flashWnd, const char* className, AdventureScreen * screen, BindsController * bindsController, VoiceOverlayController * launcherBridge );
  ~TrainingPanelController();

  // the panel is shown in this battle (shipping: the training map with one human player)
  static bool IsAvailable( NWorld::PFWorld const * world, int localUserId );

  void Update( float deltaTime );

private:
  virtual void OnFSCommand( UI::FlashContainer2* _wnd, const char* listenerID, const char* args, const wchar_t * argsW );

  void SendHeroes( NWorld::PFBaseHero const * pHero );
  void SendState( NWorld::PFBaseHero const * pHero, bool force );
  void SendUnits( bool force );
  void Send( const char * action, NWorld::PFBaseUnit * target, const char * param1 = 0, const char * param2 = 0, const char * param3 = 0 );
  void LearnNextTalent();
  void SendLibrary( int level, int slot );
  void UpdateLoadingAssertion( float deltaTime );
  void SetGridOpen( bool open );
  void RequestHeroBuild( int index );
  void UpdateHeroBuild( float deltaTime );
  void SendHero( int index, const Training::WebBuild * build );

  Weak<UI::FlashContainer2> flashContainer;
  AdventureScreen * screen;
  Weak<BindsController> bindsController; // the game keys are off while the choice window is open (its search field)
  bool shown;
  float timeToState;
  // icons of heroes and skins, a new hero or skin were not preloaded: no "loading after game start" asserts
  // while the choice window is open and a few seconds after the panel shows or acts
  bool gridOpen;
  float loadingTime;
  bool loadingAllowed;
  bool oldAssertionValue;
  CPtr<NWorld::PFBaseHero> lastHero;
  wstring lastState;
  wstring lastUnits;
  // the player's new hero chosen in the list waits for the player's build from the launcher
  struct BuildRequest
  {
    int id;        // 0 = no request
    int heroIndex;
    string heroId; // persistentId
    float time;    // left to wait

    BuildRequest() : id( 0 ), heroIndex( -1 ), time( 0.0f ) {}
  };
  Weak<VoiceOverlayController> launcherBridge;
  BuildRequest request;
  // the builds already received, by the hero persistentId: the same hero again goes at once, without the back end
  // (the build does not change during the battle - the launcher is hidden)
  nstl::map<string, Training::WebBuild> buildCache;
};

}

#endif //TRAININGPANELCONTROLLER_H_INCLUDED
