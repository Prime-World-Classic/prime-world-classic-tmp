 
 CREEP_STAT_BUDGET_PARAMS = {
	x1 = 2,
	y1 = 4,
	x2 = 36,
	y2 = 6,
	x_max = 23,
	
	y_max = 70, -- основная настройка сложности. 48 - очень просто. 72 - очень сложно
}
 
 TOWER_LIFE_INCREMENT = 8000
 RED_DIFFICULTY = true -- всё, что включено на красной, работает и на чёрной
 BLACK_DIFFICULTY = true
 INIT_CREEP_STAT_MODIFIER = 3.625 -- красная (1.45) x2.5: все статы чуди x2.5

  -- Тут были оранжевые талантсеты с 9 звездами
 
  ENEMY_HERO_REFINE_RATE = 1
 STATS_WEREWOLF = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
  bonus = {16755, 705, 312, 278, 270, 160, 462, 125} }
 STATS_FAIRY = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
  bonus = {17968, 250, 680, 415, 288, 458, 58, 2030}} 
 STATS_NAGA = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
  bonus = {19292, 385, 250, 212, 780, 205, 185, 2388} }
 STATS_WITCHDOCTOR = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
  bonus = {17298, 250, 835, 290, 242, 470, 140, 2038} }
 STATS_MANAWYRM = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
  bonus = {13350, 250, 872, 222, 280, 380, 25, 7105} }
 STATS_HUNTER = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
  bonus = {16392, 802, 250, 380, 338, 425, 50, 345} }

ENEMY_HERO_STATS_BONUS = { stats = {StatLife, StatStrength, StatIntellect, StatStamina, StatWill, StatDexterity, StatAttackSpeed, StatEnergy }, 
						   bonus = {0,     0, 			0,			   0,	 		0,		  0, 			 0, 			 0 } }

 FIRST_WAVE_HEROES = {1,2,3}
 SECOND_WAVE_HEROES = {1,2,3,4}
 
 WAVE_SPAWN_INTERVALS = { 12, 13, 14, 14 }
 WAVE_SPAWN_INTERVALS_MAIN_DIR = { 9, 9, 10, 10 }
 
 LuaDebugTrace("Difficulty is BLACK")
 
 include ("Maps/Multiplayer/Myth/script.lua")