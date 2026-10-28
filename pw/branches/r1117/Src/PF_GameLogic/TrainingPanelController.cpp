#include "stdafx.h"
#include "TrainingPanelController.h"

#include "AdventureScreen.h"
#include "PFWorld.h"
#include "PFPlayer.h"
#include "PFMaleHero.h"
#include "PFTalent.h"
#include "PFTraining.h"
#include "PFResourcesCollection.h"
#include "DBTalent.h"
#include "DBHeroesList.h"
#include "DBAdvMap.h"
#include "libdb/DbResourceCache.h"

namespace
{
  const char szTrainingCommand[] = "TrainingPanelCmd";
  const char szHeroesDb[] = "Heroes/_.HRDB.xdb"; // the list cheat hero_change takes hero indices from

  // the state goes to Flash at most this often (only when it changed)
  const float STATE_PERIOD = 0.2f;
  // icons load while the choice window is open and this long after the panel shows or acts
  const float LOADING_TIME = 5.0f;

  static bool g_trainingPanel = true;
  static bool g_trainingPanelCollapsed = false;

  const char * const s_modes[] = { "NoDamage", "NoCooldown", "NoMana" };
  const int MODES_COUNT = sizeof( s_modes ) / sizeof( s_modes[0] );

  wstring TexturePath( const NDb::Texture * texture )
  {
    return texture ? NStr::ToUnicode( texture->textureFileName ) : wstring();
  }

  bool IsSideA( NWorld::PFBaseHero const * pHero )
  {
    return pHero->GetOriginalFaction() != NDb::FACTION_BURN;
  }

  wstring HeroName( const NDb::BaseHero * pDbHero, bool sideA )
  {
    const wstring & name = sideA ? pDbHero->heroNameA.GetText() : pDbHero->heroNameB.GetText();
    if ( !name.empty() )
      return name;
    return sideA ? pDbHero->heroNameB.GetText() : pDbHero->heroNameA.GetText();
  }

  const NDb::Texture * HeroImage( const NDb::BaseHero * pDbHero, bool sideA )
  {
    const NDb::Texture * image = sideA ? pDbHero->heroImageA : pDbHero->heroImageB;
    return image ? image : ( sideA ? pDbHero->heroImageB : pDbHero->heroImageA );
  }

  bool IsSafeParam( const char * text )
  {
    if ( !text || !*text || strlen( text ) > 64 )
      return false;
    for ( const char * c = text; *c; ++c )
      if ( !isalnum( (unsigned char)*c ) && *c != '_' && *c != '-' && *c != '.' )
        return false;
    return true;
  }
}

namespace NGameX
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TrainingPanelController::TrainingPanelController( UI::FlashContainer2 * flashWnd, const char* className, AdventureScreen * _screen, BindsController * _bindsController ) :
  FlashInterface( flashWnd, className ),
  flashContainer( flashWnd ),
  screen( _screen ),
  bindsController( _bindsController ),
  shown( false ),
  timeToState( 0.0f ),
  gridOpen( false ),
  loadingTime( 0.0f ),
  loadingAllowed( false ),
  oldAssertionValue( false )
{
  flashWnd->AddFSListner( szTrainingCommand, this );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TrainingPanelController::~TrainingPanelController()
{
  if ( flashContainer )
    flashContainer->RemoveFSListner( szTrainingCommand );
  if ( gridOpen && IsValid( bindsController ) )
    bindsController->SetEnabled( BindsController::TrainingGrid, true );
  if ( loadingAllowed )
    NDb::GetDbResourceCache().EnableAssertionLoadingFiles( oldAssertionValue );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TrainingPanelController::UpdateLoadingAssertion( float deltaTime )
{
  loadingTime = max( 0.0f, loadingTime - deltaTime );
  const bool allow = gridOpen || loadingTime > 0.0f;
  if ( allow && !loadingAllowed )
    oldAssertionValue = NDb::GetDbResourceCache().EnableAssertionLoadingFiles( false );
  else if ( !allow && loadingAllowed )
    NDb::GetDbResourceCache().EnableAssertionLoadingFiles( oldAssertionValue );
  loadingAllowed = allow;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TrainingPanelController::SetGridOpen( bool open )
{
  if ( open != gridOpen && IsValid( bindsController ) )
    bindsController->SetEnabled( BindsController::TrainingGrid, !open );
  gridOpen = open;
  UpdateLoadingAssertion( 0.0f );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TrainingPanelController::Update( float deltaTime )
{
  NWorld::PFBaseHero * pHero = screen ? screen->GetHero() : 0;

  if ( !g_trainingPanel || !IsValid( pHero ) )
  {
    if ( shown )
    {
      shown = false;
      SetGridOpen( false );
      CallMethod( "ShowTrainingPanel", false, g_trainingPanelCollapsed );
    }
    UpdateLoadingAssertion( deltaTime );
    return;
  }

  // a new hero (hero_change): the list is in the colours of its side, the state is sent at once;
  // the portraits of all heroes and the icons of the new hero were not preloaded
  const bool heroChanged = pHero != lastHero;
  if ( heroChanged || !shown )
  {
    loadingTime = LOADING_TIME;
    UpdateLoadingAssertion( 0.0f );
  }
  if ( heroChanged )
  {
    lastHero = pHero;
    SendHeroes( pHero );
  }

  timeToState -= deltaTime;
  if ( heroChanged || !shown || timeToState <= 0.0f )
  {
    timeToState = STATE_PERIOD;
    SendState( pHero, heroChanged || !shown );
    SendUnits( heroChanged || !shown );
  }

  if ( !shown )
  {
    shown = true;
    CallMethod( "ShowTrainingPanel", true, g_trainingPanelCollapsed );
  }

  UpdateLoadingAssertion( deltaTime );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TrainingPanelController::SendHeroes( NWorld::PFBaseHero const * pHero )
{
  NDb::Ptr<NDb::HeroesDB> pHeroes = NDb::Get<NDb::HeroesDB>( NDb::DBID( szHeroesDb ) );
  if ( !pHeroes )
    return;

  const bool sideA = IsSideA( pHero );
  nstl::vector<int> indices;
  nstl::vector<wstring> names, icons;
  for ( int i = 0; i < pHeroes->heroes.size(); ++i )
  {
    const NDb::Hero * pDbHero = pHeroes->heroes[i];
    if ( !pDbHero || !pDbHero->legal )
      continue;
    const wstring name = HeroName( pDbHero, sideA );
    if ( name.empty() )
      continue;
    indices.push_back( i );
    names.push_back( name );
    icons.push_back( TexturePath( HeroImage( pDbHero, sideA ) ) );
  }

  CallMethod( "SetTrainingHeroes", indices, names, icons );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TrainingPanelController::SendState( NWorld::PFBaseHero const * pHero, bool force )
{
  const bool sideA = IsSideA( pHero );
  const NDb::BaseHero * pDbHero = dynamic_cast<const NDb::BaseHero *>( pHero->DbUnitDesc() );
  if ( !pDbHero )
    return;

  // hero index in the list of hero_change
  int heroIndex = -1;
  if ( NDb::Ptr<NDb::HeroesDB> pHeroes = NDb::Get<NDb::HeroesDB>( NDb::DBID( szHeroesDb ) ) )
    for ( int i = 0; i < pHeroes->heroes.size(); ++i )
      if ( pHeroes->heroes[i] && pHeroes->heroes[i]->persistentId == pDbHero->persistentId )
        heroIndex = i;

  // talents: learned / all, and how many can be learned now (the shop and the money aside)
  int learned = 0, total = 0, available = 0;
  if ( NWorld::PFBaseMaleHero const * pMale = dynamic_cast<NWorld::PFBaseMaleHero const *>( pHero ) )
  {
    const int devLevel = pMale->GetTalentsSet() ? pMale->GetTalentsSet()->GetLevelOfDevelopment() : 0;
    for ( int level = 0; level < NDb::KnownEnum<NDb::ETalentLevel>::sizeOf; ++level )
      for ( int slot = 0; slot < NDb::KnownEnum<NDb::ETalentSlot>::sizeOf; ++slot )
        if ( NWorld::PFTalent const * talent = pMale->GetTalent( level, slot ) )
        {
          ++total;
          if ( talent->IsActivated() )
            ++learned;
          else if ( level <= devLevel && talent->CanBeActivated() )
            ++available;
        }
  }

  int modes = 0;
  for ( int i = 0; i < MODES_COUNT; ++i )
    if ( pHero->GetVariableValue( NStr::StrFmt( "Cheat%s", s_modes[i] ) ) > 0.0f )
      modes |= 1 << i;

  // skins: the hero itself ("default") and its skins
  nstl::vector<wstring> skinIds, skinNames, skinIcons;
  skinIds.push_back( L"default" );
  skinNames.push_back( HeroName( pDbHero, sideA ) );
  skinIcons.push_back( TexturePath( HeroImage( pDbHero, sideA ) ) );
  int currentSkin = 0;
  const NDb::HeroSkin * pCurrentSkin = pHero->GetHeroSkin();
  for ( int i = 0; i < pDbHero->heroSkins.size(); ++i )
  {
    const NDb::HeroSkin * pSkin = pDbHero->heroSkins[i];
    if ( !pSkin || pSkin->persistentId.empty() || !IsSafeParam( pSkin->persistentId.c_str() ) )
      continue;
    if ( pSkin == pCurrentSkin )
      currentSkin = skinIds.size();
    skinIds.push_back( NStr::ToUnicode( pSkin->persistentId ) );
    skinNames.push_back( pSkin->heroName.GetText().empty() ? NStr::ToUnicode( pSkin->persistentId ) : pSkin->heroName.GetText() );
    const NDb::Texture * image = sideA ? pSkin->heroImageA : pSkin->heroImageB;
    if ( !image )
      image = sideA ? pSkin->heroImageB : pSkin->heroImageA;
    skinIcons.push_back( TexturePath( image ? image : HeroImage( pDbHero, sideA ) ) );
  }

  wstring state = NStr::StrFmtW( L"%d %d %d %d %d %d %d", heroIndex, learned, total, available, modes, currentSkin, skinIds.size() );
  for ( int i = 0; i < skinIds.size(); ++i )
  {
    state += L" ";
    state += skinIds[i];
  }
  if ( !force && state == lastState )
    return;
  lastState = state;

  CallMethod( "SetTrainingState", heroIndex, learned, total, available, modes, skinIds, skinNames, skinIcons, currentSkin );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The selected unit (removal) and the target dummy: the selected one, or the one hit last
void TrainingPanelController::SendUnits( bool force )
{
  NWorld::PFBaseUnit * pSelected = screen ? screen->GetCurrentSelectedObject() : 0;
  NWorld::PFBaseHero * pHero = screen ? screen->GetHero() : 0;

  wstring selectedName;
  bool canRemove = false;
  if ( IsValid( pSelected ) && pSelected != pHero )
  {
    NWorld::PFBaseHero const * pSelectedHero = dynamic_cast<NWorld::PFBaseHero const *>( pSelected );
    const NDb::BaseHero * pDbHero = pSelectedHero ? dynamic_cast<const NDb::BaseHero *>( pSelectedHero->DbUnitDesc() ) : 0;
    selectedName = pDbHero ? HeroName( pDbHero, IsSideA( pSelectedHero ) ) : pSelected->GetDescription();
    canRemove = pSelectedHero && ( pSelected->GetVariableValue( Training::VAR_UNIT ) > 0.0f
#ifndef _SHIPPING
      || ( pSelectedHero->GetPlayer() && pSelectedHero->GetPlayer()->IsBot() )
#endif
      );
  }

  // control (stage 3): the selected hero can be taken (a bot's one or added by the training); controlling another one
  const bool canControl = IsValid( pSelected ) && pSelected != pHero && Training::CanControl( pSelected, screen->GetOwnHero() );
  const bool controlling = screen && screen->IsControllingOtherHero();

  NWorld::PFBaseUnit * pDummy = IsValid( pSelected ) && pSelected->GetVariableValue( Training::VAR_DUMMY ) > 0.0f ? pSelected : Training::GetLastHitDummy();
  Training::DummyStats stats;
  const bool hasDummy = Training::GetDummyStats( pDummy, stats );

  const wstring units = NStr::StrFmtW( L"%s|%d|%d|%d|%d|%d|%d|%d|%d", selectedName.c_str(), canRemove ? 1 : 0, hasDummy ? 1 : 0,
    hasDummy ? (int)stats.total : 0, hasDummy ? (int)stats.dps : 0, hasDummy ? (int)stats.last : 0, hasDummy ? stats.hits : 0,
    canControl ? 1 : 0, controlling ? 1 : 0 );
  if ( !force && units == lastUnits )
    return;
  lastUnits = units;

  CallMethod( "SetTrainingUnits", selectedName, canRemove, hasDummy,
    hasDummy ? (int)stats.total : 0, hasDummy ? (int)( stats.dps + 0.5f ) : 0, hasDummy ? (int)( stats.last + 0.5f ) : 0, hasDummy ? stats.hits : 0,
    canControl, controlling );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TrainingPanelController::Send( const char * action, NWorld::PFBaseUnit * target, const char * param1, const char * param2, const char * param3 )
{
  NWorld::PFBaseHero * pHero = screen ? screen->GetHero() : 0;
  if ( !IsValid( pHero ) )
    return;

  Training::Send( action, target, pHero->GetPosition(), param1, param2, param3 );
  timeToState = 0.0f; // the result shows at once
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Talent library (stage 4): talents for a slot of the build - the class talents of the hero in this row (its talent
// sets) first, then the common talents of the row (all talents of the game except class ones), rarer first
void TrainingPanelController::SendLibrary( int level, int slot )
{
  NWorld::PFBaseMaleHero * pMale = screen ? dynamic_cast<NWorld::PFBaseMaleHero *>( screen->GetHero() ) : 0;
  NWorld::PFWorld * pWorld = screen ? screen->GetWorld() : 0;
  if ( !pMale || !pWorld || !pWorld->GetResourcesCollection() )
    return;
  if ( level < 0 || level >= NDb::KnownEnum<NDb::ETalentLevel>::sizeOf || slot < 0 || slot >= NDb::KnownEnum<NDb::ETalentSlot>::sizeOf )
    return;

  // the native talent taken on start (passive A0) is not replaced
  NWorld::PFTalent const * pSlotTalent = pMale->GetTalent( level, slot );
  if ( pSlotTalent && pSlotTalent->GetTalentDesc() && pSlotTalent->GetTalentDesc()->isTakenOnStart )
    return;

  struct Entry
  {
    const NDb::Talent * talent;
    int order; // class talents first, then by rarity (rarer first)
  };
  nstl::vector<Entry> entries;
  // talents already in the other slots of the set are not offered: a talent is in the set once
  nstl::set<string> added;
  for ( int l = 0; l < NDb::KnownEnum<NDb::ETalentLevel>::sizeOf; ++l )
    for ( int s = 0; s < NDb::KnownEnum<NDb::ETalentSlot>::sizeOf; ++s )
      if ( l != level || s != slot )
        if ( NWorld::PFTalent const * pOther = pMale->GetTalent( l, s ) )
          if ( pOther->GetTalentDesc() )
            added.insert( pOther->GetTalentDesc()->persistentId );

  if ( const NDb::Hero * pDbHero = dynamic_cast<const NDb::Hero *>( pMale->DbUnitDesc() ) )
    for ( int s = 0; s < pDbHero->defaultTalentsSets.size(); ++s )
      if ( const NDb::TalentsSet * pSet = pDbHero->defaultTalentsSets[s] )
        if ( level < pSet->levels.size() )
          for ( int i = 0; i < pSet->levels[level].talents.size(); ++i )
            if ( const NDb::Talent * pTalent = pSet->levels[level].talents[i].talent )
              if ( !pTalent->isTakenOnStart && !pTalent->persistentId.empty() && added.insert( pTalent->persistentId ).second )
              {
                Entry entry = { pTalent, 0 };
                entries.push_back( entry );
              }

  const NWorld::PFResourcesCollection::TalentMap & talents = pWorld->GetResourcesCollection()->GetTalents();
  for ( NWorld::PFResourcesCollection::TalentMap::const_iterator it = talents.begin(); it != talents.end(); ++it )
  {
    const NDb::Talent * pTalent = it->second;
    if ( !pTalent || pTalent->isTakenOnStart || pTalent->rarity == NDb::TALENTRARITY_CLASS || pTalent->minLevel != level || pTalent->persistentId.empty() )
      continue;
    if ( !added.insert( pTalent->persistentId ).second )
      continue;
    Entry entry = { pTalent, 100 - (int)pTalent->rarity };
    entries.push_back( entry );
  }

  // by the order, then by the name (selection sort: a couple of hundred talents)
  for ( int i = 0; i < entries.size(); ++i )
  {
    int best = i;
    for ( int j = i + 1; j < entries.size(); ++j )
      if ( entries[j].order < entries[best].order ||
        ( entries[j].order == entries[best].order && entries[j].talent->name.GetText() < entries[best].talent->name.GetText() ) )
        best = j;
    if ( best != i )
    {
      const Entry tmp = entries[i];
      entries[i] = entries[best];
      entries[best] = tmp;
    }
  }

  nstl::vector<wstring> ids, names, icons;
  nstl::vector<int> rarities;
  for ( int i = 0; i < entries.size(); ++i )
  {
    const NDb::Talent * pTalent = entries[i].talent;
    ids.push_back( NStr::ToUnicode( pTalent->persistentId ) );
    names.push_back( pTalent->name.GetText().empty() ? NStr::ToUnicode( pTalent->persistentId ) : pTalent->name.GetText() );
    icons.push_back( TexturePath( pTalent->image ) );
    rarities.push_back( (int)pTalent->rarity );
  }

  NWorld::PFTalent const * pCurrent = pMale->GetTalent( level, slot );
  const wstring currentId = pCurrent && pCurrent->GetTalentDesc() ? NStr::ToUnicode( pCurrent->GetTalentDesc()->persistentId ) : wstring();

  CallMethod( "ShowTrainingLibrary", level, slot, ids, names, icons, rarities, currentId );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TrainingPanelController::IsAvailable( NWorld::PFWorld const * world, int localUserId )
{
  return Training::IsAllowed( world, localUserId );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TrainingPanelController::LearnNextTalent()
{
  NWorld::PFBaseMaleHero const * pMale = screen ? dynamic_cast<NWorld::PFBaseMaleHero const *>( screen->GetHero() ) : 0;
  if ( !pMale || !pMale->GetTalentsSet() )
    return;

  // the first talent the hero could buy now (its row is open, it is not learned yet): rows from the first one,
  // in a row from left to right
  const int devLevel = pMale->GetTalentsSet()->GetLevelOfDevelopment();
  for ( int level = 0; level < NDb::KnownEnum<NDb::ETalentLevel>::sizeOf && level <= devLevel; ++level )
    for ( int slot = 0; slot < NDb::KnownEnum<NDb::ETalentSlot>::sizeOf; ++slot )
      if ( NWorld::PFTalent const * talent = pMale->GetTalent( level, slot ) )
        if ( !talent->IsActivated() && talent->CanBeActivated() )
        {
          // an active one goes to the first free button of the action bar, not to its place of the build
          CallMethod( "SetTalentAnyPosition", slot, level );
          const string levelText = NStr::StrFmt( "%d", level );
          const string slotText = NStr::StrFmt( "%d", slot );
          Send( "learn_talent", screen->GetHero(), levelText.c_str(), slotText.c_str() );
          return;
        }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TrainingPanelController::OnFSCommand( UI::FlashContainer2* _wnd, const char* listenerID, const char* args, const wchar_t * argsW )
{
  if ( strcmp( listenerID, szTrainingCommand ) != 0 || !args || strlen( args ) > 128 )
    return;

  // Flash sends: "talent_next", "talents_all", "refresh", "mode <NoDamage|NoCooldown|NoMana> <0|1>", "hero <index>",
  // "skin <id>", "collapsed <0|1>", "grid <0|1>" (the choice window), "dbg <text>"
  loadingTime = LOADING_TIME;
  UpdateLoadingAssertion( 0.0f );

  if ( strncmp( args, "dbg ", 4 ) == 0 )
    MessageTrace( "training panel: %s", args + 4 );
  else if ( strncmp( args, "grid ", 5 ) == 0 )
    SetGridOpen( atoi( args + 5 ) != 0 );
  else if ( strcmp( args, "talent_next" ) == 0 )
    LearnNextTalent();
  else if ( strcmp( args, "talents_all" ) == 0 )
    Send( "talents_all", screen->GetHero() );
  else if ( strcmp( args, "refresh" ) == 0 )
    Send( "refresh", screen->GetHero() );
  else if ( strncmp( args, "mode ", 5 ) == 0 )
  {
    char mode[32] = "";
    int on = 0;
    if ( sscanf( args + 5, "%31s %d", mode, &on ) != 2 )
      return;
    for ( int i = 0; i < MODES_COUNT; ++i )
      if ( strcmp( mode, s_modes[i] ) == 0 )
        Send( "mode", screen->GetHero(), s_modes[i], on ? "1" : "0" );
  }
  else if ( strncmp( args, "hero ", 5 ) == 0 )
  {
    const int index = atoi( args + 5 );
    NDb::Ptr<NDb::HeroesDB> pHeroes = NDb::Get<NDb::HeroesDB>( NDb::DBID( szHeroesDb ) );
    if ( pHeroes && index >= 0 && index < pHeroes->heroes.size() && pHeroes->heroes[index] )
      Send( "hero", screen->GetHero(), NStr::StrFmt( "%d", index ) );
  }
  else if ( strncmp( args, "skin ", 5 ) == 0 )
  {
    if ( IsSafeParam( args + 5 ) )
      Send( "skin", screen->GetHero(), args + 5 );
  }
  else if ( strncmp( args, "add ", 4 ) == 0 )
  {
    // "add <hero index> ally|enemy|dummy"
    int index = -1;
    char side[16] = "";
    if ( sscanf( args + 4, "%d %15s", &index, side ) != 2 )
      return;
    NDb::Ptr<NDb::HeroesDB> pHeroes = NDb::Get<NDb::HeroesDB>( NDb::DBID( szHeroesDb ) );
    if ( pHeroes && index >= 0 && index < pHeroes->heroes.size() && pHeroes->heroes[index]
      && ( strcmp( side, "ally" ) == 0 || strcmp( side, "enemy" ) == 0 || strcmp( side, "dummy" ) == 0 ) )
    {
      const string indexText = NStr::StrFmt( "%d", index );
      Send( "add", screen->GetHero(), indexText.c_str(), side );
    }
  }
  else if ( strncmp( args, "library ", 8 ) == 0 )
  {
    // right click on a talent of the build: "library <slot> <level>" (column / row of the build)
    int slot = -1, level = -1;
    if ( sscanf( args + 8, "%d %d", &slot, &level ) == 2 )
      SendLibrary( level, slot );
  }
  else if ( strncmp( args, "set_talent ", 11 ) == 0 )
  {
    // "set_talent <level> <slot> <talent persistentId>"
    int level = -1, slot = -1;
    char talentId[96] = "";
    if ( sscanf( args + 11, "%d %d %95s", &level, &slot, talentId ) == 3 && IsSafeParam( talentId ) )
    {
      const string levelText = NStr::StrFmt( "%d", level );
      const string slotText = NStr::StrFmt( "%d", slot );
      Send( "set_talent", screen->GetHero(), levelText.c_str(), slotText.c_str(), talentId );
    }
  }
  else if ( strcmp( args, "control" ) == 0 )
  {
    // the controls switch at once on this client; the world command takes the AI off a bot's hero
    NWorld::PFBaseUnit * pSelected = screen->GetCurrentSelectedObject();
    if ( IsValid( pSelected ) && pSelected != screen->GetHero() && Training::CanControl( pSelected, screen->GetOwnHero() ) )
    {
      Send( "control", pSelected );
      screen->SetControlledHero( dynamic_cast<NWorld::PFBaseHero *>( pSelected ) );
    }
  }
  else if ( strcmp( args, "release" ) == 0 )
  {
    screen->SetControlledHero( 0 );
    timeToState = 0.0f;
  }
  else if ( strcmp( args, "remove" ) == 0 )
  {
    NWorld::PFBaseUnit * pSelected = screen->GetCurrentSelectedObject();
    if ( IsValid( pSelected ) && pSelected != screen->GetHero() )
      Send( "remove", pSelected );
  }
  else if ( strcmp( args, "dummy_reset" ) == 0 )
  {
    NWorld::PFBaseUnit * pSelected = screen->GetCurrentSelectedObject();
    NWorld::PFBaseUnit * pDummy = IsValid( pSelected ) && pSelected->GetVariableValue( Training::VAR_DUMMY ) > 0.0f ? pSelected : Training::GetLastHitDummy();
    if ( pDummy )
      Send( "dummy_reset", pDummy );
  }
  else if ( strncmp( args, "collapsed ", 10 ) == 0 )
    g_trainingPanelCollapsed = atoi( args + 10 ) != 0;
}

}

REGISTER_VAR( "training_panel", g_trainingPanel, STORAGE_USER );
REGISTER_VAR( "training_panel_collapsed", g_trainingPanelCollapsed, STORAGE_USER );
