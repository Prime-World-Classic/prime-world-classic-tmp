#pragma once

namespace NWorld
{
  class PFWorld;
  class PFBaseUnit;
  class PFBaseHero;
  class PFBaseMaleHero;
}

namespace NDb
{
  struct Hero;
}

// Training panel cheats (TrainingPanelController): the world command CmdTraining. Builds with cheats run it anywhere;
// shipping builds only on the training map (MAPTYPE_TRAINING) with the sender as its only human player - every client
// checks the same, so the command does nothing (and the same nothing) in any other battle.
namespace NGameX
{
namespace Training
{
  // unit variables of training modes and units
  extern const char * const VAR_NO_DAMAGE;    // takes no damage
  extern const char * const VAR_NO_COOLDOWN;  // abilities have no cooldowns
  extern const char * const VAR_NO_MANA;      // abilities cost nothing
  extern const char * const VAR_DUMMY;        // target dummy: damage is counted, health does not go down
  extern const char * const VAR_UNIT;         // added by the training (can be removed)

  // modes are looked up only after the training turned one on: no string lookups in usual battles
  extern bool g_modesUsed;
  bool HasModeSlow( NWorld::PFBaseUnit const * unit, const char * var );
  inline bool HasMode( NWorld::PFBaseUnit const * unit, const char * var ) { return g_modesUsed && HasModeSlow( unit, var ); }

  // damage to a dummy (PFBaseUnit::OnDamage): counted in its unit variables, health stays
  void OnDummyDamage( NWorld::PFBaseUnit * dummy, float damage );

  struct DummyStats
  {
    float total;   // damage of the current series of hits
    float dps;     // total over the time of the series (at least one second)
    float last;    // the last hit
    int hits;
  };
  bool GetDummyStats( NWorld::PFBaseUnit const * dummy, DummyStats & stats );
  // the dummy hit last (client, for the panel)
  NWorld::PFBaseUnit * GetLastHitDummy();

  bool IsAllowed( NWorld::PFWorld const * world, int userId );

  // a hero added by the training: its commands are accepted from the user (HeroActions CheckHero)
  bool IsControllableUnit( NWorld::PFBaseHero const * hero, int userId );
  // the training panel may give the control of this hero to the player (client): added by the training or a bot's one
  bool CanControl( NWorld::PFBaseUnit const * unit, NWorld::PFBaseHero const * ownHero );

  // client: sends the training command about a unit (and a point on the map)
  void Send( const char * action, NWorld::PFBaseUnit * target, const CVec3 & pos, const char * param1 = 0, const char * param2 = 0, const char * param3 = 0 );

  // shared with the console cheats hero_change and set_skin (PFCheat.cpp)
  NWorld::PFBaseHero * ChangeHero( NWorld::PFWorld * world, NWorld::PFBaseHero * oldHero, const NDb::Hero * dbHero );
  bool SetSkin( NWorld::PFBaseMaleHero * hero, const string & skinId );
}
}
