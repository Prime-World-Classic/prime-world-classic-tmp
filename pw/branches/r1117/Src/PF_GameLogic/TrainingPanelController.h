#ifndef TRAININGPANELCONTROLLER_H_INCLUDED
#define TRAININGPANELCONTROLLER_H_INCLUDED

#include "UI/FlashInterface.h"
#include "UI/FSCommandListner.h"
#include "UI/FlashContainer2.h"

namespace NWorld
{
  class PFWorld;
  class PFBaseUnit;
  class PFBaseHero;
}

namespace NGameX
{

class AdventureScreen;

// Training panel of the combat UI (main.swf, Training/TrainingPanel.as): learn a random talent / all talents, refresh,
// training modes (no damage, no cooldowns, no mana), hero and skin change, heroes added on the player's side or
// against it, a target dummy with a damage counter, removal of the selected added hero. The panel collapses to a small
// tab at the left edge of the screen (user var training_panel_collapsed).
// Buttons come back as FSCommand "TrainingPanelCmd" and go to the world as the command CmdTraining (PFTraining.cpp):
// builds with cheats have the panel in any battle, shipping builds only on the training map with one human player.
class TrainingPanelController: public UI::FlashInterface, public UI::IFSCommandListner
{
  NI_DECLARE_REFCOUNT_CLASS_2( TrainingPanelController, UI::FlashInterface, UI::IFSCommandListner )

public:
  TrainingPanelController( UI::FlashContainer2 * flashWnd, const char* className, AdventureScreen * screen );
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
  void LearnRandomTalent();
  void SendLibrary( int level, int slot );
  void UpdateLoadingAssertion( float deltaTime );

  Weak<UI::FlashContainer2> flashContainer;
  AdventureScreen * screen;
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
};

}

#endif //TRAININGPANELCONTROLLER_H_INCLUDED
