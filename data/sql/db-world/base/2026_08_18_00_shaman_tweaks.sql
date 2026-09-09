-- ---------------------------------------------------------------------------
-- mod-shaman-tweaks  world SQL
-- Applied automatically by AzerothCore's db_assembler on worldserver start.
-- Safe to re-run (deletes previous module rows first).
-- ---------------------------------------------------------------------------

-- Custom Lava Burst ranks 1-7. Server-side spells cloned from vanilla 51505
-- by the C++ module after load. The client still needs a Spell.dbc patch
-- (see README) for names, icons, and the shaman spellbook tab.

DELETE FROM `spell_dbc` WHERE `ID` BETWEEN 910001 AND 910007;

INSERT INTO `spell_dbc`
(`ID`, `CastingTimeIndex`, `RecoveryTime`, `InterruptFlags`,
 `BaseLevel`, `SpellLevel`, `PowerType`, `RangeIndex`, `Speed`,
 `EquippedItemClass`,
 `Effect_1`, `EffectDieSides_1`, `EffectBasePoints_1`, `ImplicitTargetA_1`,
 `SpellIconID`,
 `Name_Lang_enUS`, `NameSubtext_Lang_enUS`, `Description_Lang_enUS`, `Name_Lang_Mask`,
 `ManaCostPct`, `StartRecoveryCategory`, `StartRecoveryTime`,
 `SpellClassSet`, `SpellClassMask_2`, `DefenseType`, `PreventionType`,
 `SchoolMask`, `EffectBonusMultiplier_1`, `EffectChainAmplitude_1`)
VALUES
(910001, 14, 8000, 10, 10, 10, 0, 4, 24, -1, 2,  15,   80, 6, 3370, 'Lava Burst', 'Rank 1', 'You hurl molten lava at the target, dealing $s1 Fire damage. If your Flame Shock is on the target, Lava Burst will deal a critical strike.', 16712190, 10, 133, 1500, 11, 4096, 1, 1, 4, 0.5714, 1),
(910002, 14, 8000, 10, 20, 20, 0, 4, 24, -1, 2,  22,  149, 6, 3370, 'Lava Burst', 'Rank 2', 'You hurl molten lava at the target, dealing $s1 Fire damage. If your Flame Shock is on the target, Lava Burst will deal a critical strike.', 16712190, 10, 133, 1500, 11, 4096, 1, 1, 4, 0.5714, 1),
(910003, 14, 8000, 10, 30, 30, 0, 4, 24, -1, 2,  42,  308, 6, 3370, 'Lava Burst', 'Rank 3', 'You hurl molten lava at the target, dealing $s1 Fire damage. If your Flame Shock is on the target, Lava Burst will deal a critical strike.', 16712190, 10, 133, 1500, 11, 4096, 1, 1, 4, 0.5714, 1),
(910004, 14, 8000, 10, 40, 40, 0, 4, 24, -1, 2,  53,  407, 6, 3370, 'Lava Burst', 'Rank 4', 'You hurl molten lava at the target, dealing $s1 Fire damage. If your Flame Shock is on the target, Lava Burst will deal a critical strike.', 16712190, 10, 133, 1500, 11, 4096, 1, 1, 4, 0.5714, 1),
(910005, 14, 8000, 10, 50, 50, 0, 4, 24, -1, 2,  77,  623, 6, 3370, 'Lava Burst', 'Rank 5', 'You hurl molten lava at the target, dealing $s1 Fire damage. If your Flame Shock is on the target, Lava Burst will deal a critical strike.', 16712190, 10, 133, 1500, 11, 4096, 1, 1, 4, 0.5714, 1),
(910006, 14, 8000, 10, 60, 60, 0, 4, 24, -1, 2, 127,  890, 6, 3370, 'Lava Burst', 'Rank 6', 'You hurl molten lava at the target, dealing $s1 Fire damage. If your Flame Shock is on the target, Lava Burst will deal a critical strike.', 16712190, 10, 133, 1500, 11, 4096, 1, 1, 4, 0.5714, 1),
(910007, 14, 8000, 10, 70, 70, 0, 4, 24, -1, 2, 146, 1014, 6, 3370, 'Lava Burst', 'Rank 7', 'You hurl molten lava at the target, dealing $s1 Fire damage. If your Flame Shock is on the target, Lava Burst will deal a critical strike.', 16712190, 10, 133, 1500, 11, 4096, 1, 1, 4, 0.5714, 1);

-- Rank chain: custom 1-7, then vanilla 51505 = rank 8, 60043 = rank 9
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (51505, 910001);
INSERT INTO `spell_ranks` (`first_spell_id`, `spell_id`, `rank`) VALUES
(910001, 910001, 1),
(910001, 910002, 2),
(910001, 910003, 3),
(910001, 910004, 4),
(910001, 910005, 5),
(910001, 910006, 6),
(910001, 910007, 7),
(910001, 51505,  8),
(910001, 60043,  9);

-- Bind the C++ script to every Lava Burst rank
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_sha_lava_burst_tweaks';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(51505,  'spell_sha_lava_burst_tweaks'),
(60043,  'spell_sha_lava_burst_tweaks'),
(910001, 'spell_sha_lava_burst_tweaks'),
(910002, 'spell_sha_lava_burst_tweaks'),
(910003, 'spell_sha_lava_burst_tweaks'),
(910004, 'spell_sha_lava_burst_tweaks'),
(910005, 'spell_sha_lava_burst_tweaks'),
(910006, 'spell_sha_lava_burst_tweaks'),
(910007, 'spell_sha_lava_burst_tweaks');

-- Spell power coefficients (copied from vanilla 51505 when that row exists)
DELETE FROM `spell_bonus_data` WHERE `entry` BETWEEN 910001 AND 910007;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`)
SELECT 910001, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, 'Lava Burst (Rank 1)' FROM `spell_bonus_data` WHERE `entry` = 51505;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`)
SELECT 910002, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, 'Lava Burst (Rank 2)' FROM `spell_bonus_data` WHERE `entry` = 51505;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`)
SELECT 910003, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, 'Lava Burst (Rank 3)' FROM `spell_bonus_data` WHERE `entry` = 51505;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`)
SELECT 910004, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, 'Lava Burst (Rank 4)' FROM `spell_bonus_data` WHERE `entry` = 51505;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`)
SELECT 910005, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, 'Lava Burst (Rank 5)' FROM `spell_bonus_data` WHERE `entry` = 51505;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`)
SELECT 910006, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, 'Lava Burst (Rank 6)' FROM `spell_bonus_data` WHERE `entry` = 51505;
INSERT INTO `spell_bonus_data` (`entry`, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, `comments`)
SELECT 910007, `direct_bonus`, `dot_bonus`, `ap_bonus`, `ap_dot_bonus`, 'Lava Burst (Rank 7)' FROM `spell_bonus_data` WHERE `entry` = 51505;

-- Offer ranks 1-7 on every trainer that already teaches vanilla Lava Burst
DELETE FROM `trainer_spell` WHERE `SpellId` BETWEEN 910001 AND 910007;

INSERT INTO `trainer_spell`
(`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`)
SELECT `TrainerId`, 910001,    400, 0, 0,      0, 0, 0, 10, 0 FROM `trainer_spell` WHERE `SpellId` = 51505;
INSERT INTO `trainer_spell`
(`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`)
SELECT `TrainerId`, 910002,   1800, 0, 0, 910001, 0, 0, 20, 0 FROM `trainer_spell` WHERE `SpellId` = 51505;
INSERT INTO `trainer_spell`
(`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`)
SELECT `TrainerId`, 910003,  10000, 0, 0, 910002, 0, 0, 30, 0 FROM `trainer_spell` WHERE `SpellId` = 51505;
INSERT INTO `trainer_spell`
(`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`)
SELECT `TrainerId`, 910004,  18000, 0, 0, 910003, 0, 0, 40, 0 FROM `trainer_spell` WHERE `SpellId` = 51505;
INSERT INTO `trainer_spell`
(`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`)
SELECT `TrainerId`, 910005,  27000, 0, 0, 910004, 0, 0, 50, 0 FROM `trainer_spell` WHERE `SpellId` = 51505;
INSERT INTO `trainer_spell`
(`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`)
SELECT `TrainerId`, 910006,  34000, 0, 0, 910005, 0, 0, 60, 0 FROM `trainer_spell` WHERE `SpellId` = 51505;
INSERT INTO `trainer_spell`
(`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`)
SELECT `TrainerId`, 910007, 100000, 0, 0, 910006, 0, 0, 70, 0 FROM `trainer_spell` WHERE `SpellId` = 51505;
