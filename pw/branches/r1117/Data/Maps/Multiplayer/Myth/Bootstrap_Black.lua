 
 CREEP_STAT_BUDGET_PARAMS = {
	x1 = 2,
	y1 = 4,
	x2 = 36,
	y2 = 6,
	x_max = 23,
	
	y_max = 70, -- основная настройка сложности. 48 - очень просто. 72 - очень сложно
}
 
 TOWER_LIFE_INCREMENT = 8000
 -- Золотая сложность. Файлы карты остаются Black.*: этот путь передаёт бэкенд (режим 4).
 RED_DIFFICULTY = true -- всё, что включено на красной, работает и на золотой
 GOLD_DIFFICULTY = true
 INIT_CREEP_STAT_MODIFIER = 2.175 -- красная (1.45) x1.5: все статы чуди x1.5

  -- Тут были оранжевые талантсеты с 9 звездами
 
  ENEMY_HERO_REFINE_RATE = 1
 STATS_WEREWOLF = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
  bonus = {10053, 423, 188, 166, 162, 96, 278, 75} }
 STATS_FAIRY = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
  bonus = {10780, 150, 408, 249, 172, 274, 34, 1218}} 
 STATS_NAGA = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
  bonus = {11576, 231, 150, 128, 468, 123, 111, 1432} }
 STATS_WITCHDOCTOR = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
  bonus = {10378, 150, 501, 174, 146, 282, 84, 1222} }
 STATS_MANAWYRM = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
  bonus = {8010, 150, 524, 134, 168, 228, 15, 4263} }
 STATS_HUNTER = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
  bonus = {9836, 482, 150, 228, 202, 255, 30, 207} }

ENEMY_HERO_STATS_BONUS = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
						   bonus = {0,     0, 			0,			   0,	 		0,		  0, 			 0, 			 0 } }

 FIRST_WAVE_HEROES = {1,2,3}
 SECOND_WAVE_HEROES = {1,2,3,4}
 
 WAVE_SPAWN_INTERVALS = { 12, 13, 14, 14 }
 WAVE_SPAWN_INTERVALS_MAIN_DIR = { 9, 9, 10, 10 }
 
 LuaDebugTrace("Difficulty is GOLD")
 
 include ("Maps/Multiplayer/Myth/script.lua")