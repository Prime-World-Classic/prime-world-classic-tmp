#include "stdafx.h"
#include "PFTraining.h"

#include "AdventureScreen.h"
#include "PFWorld.h"
#include "PFPlayer.h"
#include "PFHero.h"
#include "PFMaleHero.h"
#include "PFTalent.h"
#include "PFResourcesCollection.h"
#include "PFBaseMovingUnit.h"
#include "DBHeroesList.h"
#include "DBAdvMap.h"
#include "libdb/DbResourceCache.h"
#include "Core/WorldCommand.h"

namespace NGameX
{
namespace Training
{

const char * const VAR_NO_DAMAGE   = "CheatNoDamage";
const char * const VAR_NO_COOLDOWN = "CheatNoCooldown";
const char * const VAR_NO_MANA     = "CheatNoMana";
const char * const VAR_DUMMY       = "TrainingDummy";
const char * const VAR_UNIT        = "TrainingUnit";

namespace
{
  const char * const VAR_DUMMY_TOTAL = "TrainingDummyTotal";
  const char * const VAR_DUMMY_HITS  = "TrainingDummyHits";
  const char * const VAR_DUMMY_LAST  = "TrainingDummyLast";
  const char * const VAR_DUMMY_FIRST = "TrainingDummyFirst";
  const char * const VAR_DUMMY_TIME  = "TrainingDummyTime";

  // a pause between hits this long starts a new series (the counter of the dummy starts from zero)
  const float SERIES_PAUSE = 5.0f;
  // added heroes appear this far in front of the hero
  const float ADD_DISTANCE = 4.0f;

  const char * const s_modes[] = { VAR_NO_DAMAGE, VAR_NO_COOLDOWN, VAR_NO_MANA };
  const char * const s_modeNames[] = { "NoDamage", "NoCooldown", "NoMana" };
  const int MODES_COUNT = sizeof( s_modes ) / sizeof( s_modes[0] );

  // variables a replaced hero passes to the new one
  const char * const s_keptVars[] = { VAR_NO_DAMAGE, VAR_NO_COOLDOWN, VAR_NO_MANA, VAR_DUMMY, VAR_UNIT };

  const char szHeroesDb[] = "Heroes/_.HRDB.xdb"; // hero indices of the panel and of hero_change

  CPtr<NWorld::PFBaseUnit> & LastHitDummy()
  {
    static CPtr<NWorld::PFBaseUnit> dummy;
    return dummy;
  }

  void SetVar( NWorld::PFBaseUnit * unit, const char * name, float value )
  {
    unit->GetVariableVWM( name )->SetBaseValue( value );
  }

  NDb::EFaction OppositeFaction( NDb::EFaction faction )
  {
    return faction == NDb::FACTION_FREEZE ? NDb::FACTION_BURN : NDb::FACTION_FREEZE;
  }

  int ParamInt( const vector<wstring> & params, int index )
  {
    return index < params.size() ? (int)NGlobal::VariantValue( params[index] ).GetFloat() : 0;
  }

  string ParamStr( const vector<wstring> & params, int index )
  {
    return index < params.size() ? NStr::ToMBCS( params[index] ) : string();
  }

  const NDb::Hero * FindDbHero( int index )
  {
    NDb::Ptr<NDb::HeroesDB> pHeroes = NDb::Get<NDb::HeroesDB>( NDb::DBID( szHeroesDb ) );
    if ( !pHeroes || index < 0 || index >= pHeroes->heroes.size() )
      return 0;
    return pHeroes->heroes[index];
  }

  // the UI of the local player follows its new hero; a controlled hero (stage 3) rebinds only the controls
  void RebindLocalUI( NWorld::PFBaseHero * pHero )
  {
    AdventureScreen * pAdvScreen = AdventureScreen::Instance();
    if ( !pAdvScreen || !pHero )
      return;
    if ( pAdvScreen->GetOwnHero() == pHero && !pAdvScreen->IsControllingOtherHero() )
    {
      pAdvScreen->SetLogicHero( pHero );
      pAdvScreen->OnHeroSetActive();
    }
    else if ( pAdvScreen->GetHero() == pHero )
      pAdvScreen->SetControlledHero( pHero );
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  bool LearnTalent( NWorld::PFBaseMaleHero * pHero, int level, int slot )
  {
    if ( !pHero || !pHero->GetTalentsSet() )
      return false;
    if ( level < 0 || level >= NDb::KnownEnum<NDb::ETalentLevel>::sizeOf || slot < 0 || slot >= NDb::KnownEnum<NDb::ETalentSlot>::sizeOf )
      return false;

    NWorld::PFTalent * const talent = pHero->GetTalent( level, slot );
    if ( !talent || talent->IsActivated() || !talent->CanBeActivated() || level > pHero->GetTalentsSet()->GetLevelOfDevelopment() )
      return false;

    pHero->AddGold( talent->GetMoneyCost() );
    return pHero->ActivateTalent( level, slot );
  }

  bool LearnAllTalents( NWorld::PFBaseMaleHero * pHero )
  {
    if ( !pHero )
      return false;
    for ( int level = 0; level < NDb::KnownEnum<NDb::ETalentLevel>::sizeOf; ++level )
      for ( int slot = 0; slot < NDb::KnownEnum<NDb::ETalentSlot>::sizeOf; ++slot )
      {
        NWorld::PFTalent * const talent = pHero->GetTalent( level, slot );
        if ( !talent || talent->IsActivated() )
          continue;
        pHero->AddGold( talent->GetMoneyCost() );
        pHero->ActivateTalent( level, slot );
      }
    return true;
  }

  // full health and mana (a dead hero comes back where it died), no cooldowns
  bool Refresh( NWorld::PFBaseHero * pHero, bool isSender )
  {
    if ( !pHero )
      return false;

    if ( pHero->IsDead() )
    {
      if ( pHero->IsResurrecting() )
        return false;
      pHero->Resurrect( NWorld::PFBaseHero::ResurrectParams( NWorld::PFBaseHero::ResurrectParams::Death ) );
    }
    else
    {
      pHero->SetHealth( pHero->GetMaxHealth() );
      pHero->ChangeMana( pHero->GetMaxMana() );
    }

    pHero->DropCooldowns( NWorld::DropCooldownParams( NDb::ABILITYIDFLAGS_ALL ) );
    if ( isSender )
      pHero->DropImpulsesCooldowns();
    return true;
  }

  bool SetMode( NWorld::PFBaseUnit * pUnit, const string & mode, bool on )
  {
    for ( int i = 0; i < MODES_COUNT; ++i )
    {
      if ( mode != s_modeNames[i] )
        continue;

      SetVar( pUnit, s_modes[i], on ? 1.0f : 0.0f );
      if ( on && s_modes[i] == VAR_NO_COOLDOWN )
        if ( NWorld::PFBaseHero * pHero = dynamic_cast<NWorld::PFBaseHero *>( pUnit ) )
          pHero->DropCooldowns( NWorld::DropCooldownParams( NDb::ABILITYIDFLAGS_ALL ) );
      if ( on && s_modes[i] == VAR_NO_MANA )
        pUnit->ChangeMana( pUnit->GetMaxMana() );
      return true;
    }
    return false;
  }

  // a hero without a player (playerId -1) on the given side: no AI, does not come back after death
  NWorld::PFBaseHero * AddHero( NWorld::PFWorld * world, const NDb::Hero * dbHero, NDb::EFaction faction, const CVec3 & pos, bool dummy )
  {
    NWorld::PFBaseHero::SpawnInfo info;
    info.pHero           = dbHero;
    info.playerId        = -1;
    info.faction         = faction;
    info.originalFaction = faction;
    info.inTeamId        = 1; // colour index, must be > 0
    info.isClone         = false;
    info.placement       = Placement( pos, QNULL, CVec3( 1.0f, 1.0f, 1.0f ) );

    // the hero was not preloaded: its files are loaded right now
    const bool oldAssertValue = NDb::GetDbResourceCache().EnableAssertionLoadingFiles( false );
    NWorld::PFBaseHero * pHero = NWorld::CreateHero( world, info );
    NDb::GetDbResourceCache().EnableAssertionLoadingFiles( oldAssertValue );

    if ( pHero )
    {
      pHero->SetForbidRespawn( true );
      SetVar( pHero, VAR_UNIT, 1.0f );
      if ( dummy )
        SetVar( pHero, VAR_DUMMY, 1.0f );
    }
    return pHero;
  }

  // in front of the hero (along its move direction)
  CVec3 PlaceInFront( NWorld::PFBaseUnit const * pUnit )
  {
    CVec3 pos = pUnit->GetPosition();
    float dx = 1.0f, dy = 0.0f;
    if ( NWorld::PFBaseMovingUnit const * pMoving = dynamic_cast<NWorld::PFBaseMovingUnit const *>( pUnit ) )
    {
      const CVec2 moveDir = pMoving->GetMoveDirection();
      const float length = sqrt( moveDir.x * moveDir.x + moveDir.y * moveDir.y );
      if ( length > 0.01f )
      {
        dx = moveDir.x / length;
        dy = moveDir.y / length;
      }
    }
    pos.x += dx * ADD_DISTANCE;
    pos.y += dy * ADD_DISTANCE;
    return pos;
  }

  // units the training may remove: added by it; bot heroes in builds with cheats
  bool CanRemove( NWorld::PFBaseUnit const * pUnit )
  {
    if ( pUnit->GetVariableValue( VAR_UNIT ) > 0.0f )
      return true;
#ifndef _SHIPPING
    if ( NWorld::PFBaseHero const * pHero = dynamic_cast<NWorld::PFBaseHero const *>( pUnit ) )
      return pHero->GetPlayer() && pHero->GetPlayer()->IsBot();
#endif
    return false;
  }

  NWorld::PFPlayer * FindHuman( NWorld::PFWorld const * world, int userId )
  {
    for ( int i = 0; i < world->GetPlayersCount(); ++i )
    {
      NWorld::PFPlayer * player = const_cast<NWorld::PFWorld *>( world )->GetPlayer( i );
      if ( player && !player->IsBot() && player->GetUserID() == userId )
        return player;
    }
    return 0;
  }
} // namespace

bool g_modesUsed =
#ifndef _SHIPPING
  true;
#else
  false;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool HasModeSlow( NWorld::PFBaseUnit const * unit, const char * var )
{
  return unit && unit->GetVariableValue( var ) > 0.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void OnDummyDamage( NWorld::PFBaseUnit * dummy, float damage )
{
  const float now = dummy->GetWorld()->GetTimeElapsed();
  if ( dummy->GetVariableValue( VAR_DUMMY_HITS ) <= 0.0f || now - dummy->GetVariableValue( VAR_DUMMY_TIME ) > SERIES_PAUSE )
  {
    SetVar( dummy, VAR_DUMMY_TOTAL, 0.0f );
    SetVar( dummy, VAR_DUMMY_HITS, 0.0f );
    SetVar( dummy, VAR_DUMMY_FIRST, now );
  }
  SetVar( dummy, VAR_DUMMY_TOTAL, dummy->GetVariableValue( VAR_DUMMY_TOTAL ) + damage );
  SetVar( dummy, VAR_DUMMY_HITS, dummy->GetVariableValue( VAR_DUMMY_HITS ) + 1.0f );
  SetVar( dummy, VAR_DUMMY_LAST, damage );
  SetVar( dummy, VAR_DUMMY_TIME, now );
  LastHitDummy() = dummy;
}

bool GetDummyStats( NWorld::PFBaseUnit const * dummy, DummyStats & stats )
{
  if ( !IsValid( dummy ) || dummy->GetVariableValue( VAR_DUMMY ) <= 0.0f )
    return false;
  stats.hits = (int)dummy->GetVariableValue( VAR_DUMMY_HITS );
  stats.total = dummy->GetVariableValue( VAR_DUMMY_TOTAL );
  stats.last = dummy->GetVariableValue( VAR_DUMMY_LAST );
  const float duration = dummy->GetVariableValue( VAR_DUMMY_TIME ) - dummy->GetVariableValue( VAR_DUMMY_FIRST );
  stats.dps = stats.hits > 0 ? stats.total / max( 1.0f, duration ) : 0.0f;
  return true;
}

NWorld::PFBaseUnit * GetLastHitDummy()
{
  return IsValid( LastHitDummy() ) ? LastHitDummy().GetPtr() : 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsAllowed( NWorld::PFWorld const * world, int userId )
{
#ifndef _SHIPPING
  return world != 0;
#else
  // the training map, the sender is the only human player there
  if ( !world || !world->GetMapDescription() || world->GetMapDescription()->mapType != NDb::MAPTYPE_TRAINING )
    return false;
  int humans = 0;
  for ( int i = 0; i < world->GetPlayersCount(); ++i )
  {
    NWorld::PFPlayer const * player = const_cast<NWorld::PFWorld *>( world )->GetPlayer( i );
    if ( player && !player->IsBot() )
      ++humans;
  }
  return humans == 1 && FindHuman( world, userId ) != 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsControllableUnit( NWorld::PFBaseHero const * hero, int userId )
{
  // the variable first: a usual battle has no such heroes, IsAllowed is not even called
  return hero && !hero->GetPlayer() && hero->GetVariableValue( VAR_UNIT ) > 0.0f && IsAllowed( hero->GetWorld(), userId );
}

bool CanControl( NWorld::PFBaseUnit const * unit, NWorld::PFBaseHero const * ownHero )
{
  NWorld::PFBaseMaleHero const * pHero = dynamic_cast<NWorld::PFBaseMaleHero const *>( unit );
  if ( !IsValid( pHero ) || pHero == ownHero || pHero->IsClone() )
    return false;
  if ( pHero->GetVariableValue( VAR_UNIT ) > 0.0f )
    return true;
  return pHero->GetPlayer() && pHero->GetPlayer()->IsBot();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::PFBaseHero * ChangeHero( NWorld::PFWorld * world, NWorld::PFBaseHero * oldHero, const NDb::Hero * dbHero )
{
  if ( !world || !IsValid( oldHero ) || !dbHero )
    return 0;

  CPtr<NWorld::PFBaseHero> pOldHero = oldHero;
  NWorld::PFPlayer * pPlayer = pOldHero->GetPlayer();

  // the player controls this hero (not its own one): the control goes to the new hero
  AdventureScreen * pAdvScreen = AdventureScreen::Instance();
  const bool wasControlled = pAdvScreen && pAdvScreen->IsControllingOtherHero() && pAdvScreen->GetHero() == oldHero;

  NWorld::PFBaseHero::SpawnInfo info;
  info.pHero           = dbHero;
  info.playerId        = pPlayer ? pPlayer->GetPlayerID() : -1;
  info.placement       = Placement( pOldHero->GetPosition(), QNULL, CVec3( 1.0f, 1.0f, 1.0f ) );
  info.faction         = pOldHero->GetFaction(); // for a hero without a player; a player gives its own faction
  info.originalFaction = pOldHero->GetOriginalFaction();
  info.inTeamId        = pOldHero->GetInTeamId() > 0 ? pOldHero->GetInTeamId() : 1;
  info.isClone         = false;

  if ( pPlayer )
  {
    // in its place in the team (as LuaHeroReplace does); DetachFromPlayer makes the old hero neutral
    info.inTeamId = 1;
    for ( int i = 0; i < world->GetPlayersCount(); ++i )
    {
      NWorld::PFPlayer * player = world->GetPlayer( i );
      if ( player == pPlayer )
        break;
      if ( player && player->GetTeamID() == pPlayer->GetTeamID() )
        ++info.inTeamId;
    }
    pOldHero->DetachFromPlayer();
    pPlayer->DetachHero();
  }

  // the new hero was not preloaded: its files are loaded right now
  const bool oldAssertValue = NDb::GetDbResourceCache().EnableAssertionLoadingFiles( false );
  NWorld::PFBaseHero * pNewHero = NWorld::CreateHero( world, info );
  NDb::GetDbResourceCache().EnableAssertionLoadingFiles( oldAssertValue );

  if ( pNewHero )
  {
    pNewHero->AddGold( pOldHero->GetGold(), false );
    for ( int i = 0; i < sizeof( s_keptVars ) / sizeof( s_keptVars[0] ); ++i )
      if ( pOldHero->GetVariableValue( s_keptVars[i] ) > 0.0f )
        SetVar( pNewHero, s_keptVars[i], 1.0f );
    if ( !pPlayer )
      pNewHero->SetForbidRespawn( true );
  }

  pOldHero->SetForbidRespawn( true );
  pOldHero->DebugDie();

  if ( wasControlled && pNewHero )
    pAdvScreen->SetControlledHero( pNewHero );
  else
    RebindLocalUI( pNewHero );
  return pNewHero;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SetSkin( NWorld::PFBaseMaleHero * hero, const string & skinId )
{
  if ( !hero )
    return false;

  // a skin that was not chosen for the session was not preloaded: its files are loaded right now
  const bool oldAssertValue = NDb::GetDbResourceCache().EnableAssertionLoadingFiles( false );
  const bool res = hero->SetSkin( skinId, true );
  NDb::GetDbResourceCache().EnableAssertionLoadingFiles( oldAssertValue );

  // the portraits of the HUD take the picture when the UI is bound to the hero
  if ( res )
    RebindLocalUI( hero );
  return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Execute( NWorld::PFWorld * world, int userId, const string & action, const vector<wstring> & params, NWorld::PFBaseUnit * pTarget, const CVec3 & pos )
{
  NWorld::PFBaseHero * pHero = dynamic_cast<NWorld::PFBaseHero *>( pTarget );
  NWorld::PFBaseMaleHero * pMale = dynamic_cast<NWorld::PFBaseMaleHero *>( pTarget );
  NWorld::PFPlayer * pSender = FindHuman( world, userId );
  const bool isSenderHero = pSender && pHero && pSender->GetHero() == pHero;

  if ( action == "learn_talent" )
    LearnTalent( pMale, ParamInt( params, 0 ), ParamInt( params, 1 ) );
  else if ( action == "talents_all" )
    LearnAllTalents( pMale );
  else if ( action == "refresh" )
    Refresh( pHero, isSenderHero );
  else if ( action == "mode" )
  {
    if ( pTarget )
      SetMode( pTarget, ParamStr( params, 0 ), ParamInt( params, 1 ) != 0 );
  }
  else if ( action == "hero" )
    ChangeHero( world, pHero, FindDbHero( ParamInt( params, 0 ) ) );
  else if ( action == "skin" )
    SetSkin( pMale, ParamStr( params, 0 ) );
  else if ( action == "add" )
  {
    // "add <hero index> ally|enemy|dummy": on the side of the target hero (the player's one) or against it
    const NDb::Hero * dbHero = FindDbHero( ParamInt( params, 0 ) );
    const string side = ParamStr( params, 1 );
    if ( dbHero && pTarget && ( side == "ally" || side == "enemy" || side == "dummy" ) )
    {
      const NDb::EFaction faction = side == "ally" ? pTarget->GetFaction() : OppositeFaction( pTarget->GetFaction() );
      AddHero( world, dbHero, faction, PlaceInFront( pTarget ), side == "dummy" );
    }
  }
  else if ( action == "remove" )
  {
    if ( pHero && !isSenderHero && CanRemove( pHero ) )
    {
      pHero->SetForbidRespawn( true );
      pHero->DebugDie();
    }
  }
  else if ( action == "set_talent" )
  {
    // "set_talent <level> <slot> <talent persistentId>": the talent library of the panel (stage 4)
    const int level = ParamInt( params, 0 );
    const int slot = ParamInt( params, 1 );
    const string talentId = ParamStr( params, 2 );
    NDb::Ptr<NDb::Talent> pDesc = world->GetResourcesCollection() ? world->GetResourcesCollection()->FindTalentById( talentId ) : 0;
    if ( pMale && pMale->GetTalentsSet() && pDesc )
    {
      // the talent was not preloaded: its files are loaded right now
      const bool oldAssertValue = NDb::GetDbResourceCache().EnableAssertionLoadingFiles( false );
      const bool replaced = pMale->GetTalentsSet()->ReplaceTalent( level, slot, pDesc );
      NDb::GetDbResourceCache().EnableAssertionLoadingFiles( oldAssertValue );
      // the build window stays open: only the controls of the current hero are filled again
      AdventureScreen * pAdvScreen = AdventureScreen::Instance();
      if ( replaced && pAdvScreen && pAdvScreen->GetHero() == pMale )
        pAdvScreen->RefreshHeroControls();
    }
  }
  else if ( action == "control" )
  {
    // the player controls this hero (the client switches its controls itself): a bot's hero loses its AI, the orders
    // would be overridden by it. Added heroes have no AI
    if ( pHero && !isSenderHero && pHero->GetPlayer() && pHero->GetPlayer()->IsBot() )
      world->RemoveAI( pHero );
  }
  else if ( action == "dummy_reset" )
  {
    if ( pTarget && pTarget->GetVariableValue( VAR_DUMMY ) > 0.0f )
    {
      SetVar( pTarget, VAR_DUMMY_HITS, 0.0f );
      SetVar( pTarget, VAR_DUMMY_TOTAL, 0.0f );
      SetVar( pTarget, VAR_DUMMY_LAST, 0.0f );
    }
  }
  else
    DebugTrace( "Unknown training command \"%s\"", action.c_str() );
}

} // namespace Training

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DEFINE_4_PARAM_CMD( 0x11629410, CmdTraining, string, action, vector<wstring>, params, CPtr<NWorld::PFBaseUnit>, target, CVec3, pos );

void CmdTraining::Execute( NCore::IWorldBase * pWorldBase )
{
  NWorld::PFWorld * world = dynamic_cast<NWorld::PFWorld *>( pWorldBase );
  if ( !Training::IsAllowed( world, GetId() ) )
  {
    DebugTrace( "Training command \"%s\" rejected (user %d)", action.c_str(), GetId() );
    return;
  }

  Training::g_modesUsed = true;
  Training::Execute( world, GetId(), action, params, IsValid( target ) ? target.GetPtr() : 0, pos );
}

namespace Training
{
void Send( const char * action, NWorld::PFBaseUnit * target, const CVec3 & pos, const char * param1, const char * param2, const char * param3 )
{
  AdventureScreen * advScreen = AdventureScreen::Instance();
  if ( !advScreen || advScreen->IsInReplayMode() )
    return;

  vector<wstring> params;
  if ( param1 )
    params.push_back( NStr::ToUnicode( param1 ) );
  if ( param2 )
    params.push_back( NStr::ToUnicode( param2 ) );
  if ( param3 )
    params.push_back( NStr::ToUnicode( param3 ) );

  advScreen->SendGameCommand( new CmdTraining( action, params, target, pos ) );
}
}

} // namespace NGameX

REGISTER_SAVELOAD_CLASS_NM( CmdTraining, NGameX )
