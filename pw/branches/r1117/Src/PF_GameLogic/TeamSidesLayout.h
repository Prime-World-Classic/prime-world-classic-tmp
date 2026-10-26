#pragma once

namespace NGameX
{

// The 5x5 look of the team panels (loading screen, ally portraits, Tab
// statistics) when our team is bigger than a panel side: the trial for 10
// players (Maps/Multiplayer/Myth) puts everybody into one team. The first
// allies stay on our side (our hero is always among them), the rest move to the
// opposite side, where an ordinary battle shows the enemies; they are still
// shown as allies. With up to SIDE_SLOTS allies nothing changes.
//
// The loading screen and the battle feed the allies in the same order (player
// id), so both get the same split.
class TeamSidesLayout
{
public:
  static const int SIDE_SLOTS = 5;

  TeamSidesLayout() : ourSide( 0 ), otherSide( 0 ), ourHeroPlaced( false ) {}

  // Call for every ally in player order; true - show this ally on the opposite side.
  bool PlaceAlly( bool isOurHero )
  {
    const int reservedForOurHero = ( ourHeroPlaced || isOurHero ) ? 0 : 1;

    if ( !isOurHero && ourSide + reservedForOurHero >= SIDE_SLOTS )
    {
      ++otherSide;
      return true;
    }

    ++ourSide;
    if ( isOurHero )
      ourHeroPlaced = true;
    return false;
  }

  int AlliesOnOtherSide() const { return otherSide; }

  // Opposite side slots that are left for the enemies.
  int EnemySlots() const { return SIDE_SLOTS - otherSide; }

private:
  int ourSide;
  int otherSide;
  bool ourHeroPlaced;
};

} // namespace NGameX
