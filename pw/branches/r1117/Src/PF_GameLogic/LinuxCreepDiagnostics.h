#pragma once

namespace NWorld
{
/** Detached native report values, sampled from the world registry without advancing it.
 * Minima include disabled/empty spawners and signed delays; absent kinds report zero.
 * Movement includes dead registered creeps and preserves registry-order float sums.
 * This is not serialized, cached across ticks, or used to decide gameplay.
 */
struct LinuxCreepDiagnostics
{
	int spawnedNeutralCreeps = 0;
	int movingCommonCreeps = 0;
	int movedCommonCreeps = 0;
	float commonCreepMovementDistance = 0;
	int creepWaves = 0;
	int neutralWaves = 0;
	int readyCreepSpawners = 0;
	int readyNeutralSpawners = 0;
	int enabledCreepSpawners = 0;
	int enabledNeutralSpawners = 0;
	int contentCreepSpawners = 0;
	int contentNeutralSpawners = 0;
	float minCreepSpawnDelay = 0;
	float minNeutralSpawnDelay = 0;
};
}
