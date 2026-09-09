-- Restore vanilla Lava Burst ranks and remove module rows.
-- Run this by hand if you remove the module. Do not put it under
-- data/sql/db-world/ — that folder is applied automatically.

DELETE FROM `trainer_spell` WHERE `SpellId` BETWEEN 910001 AND 910007;
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_sha_lava_burst_tweaks';
DELETE FROM `spell_bonus_data` WHERE `entry` BETWEEN 910001 AND 910007;
DELETE FROM `spell_ranks` WHERE `first_spell_id` IN (51505, 910001);
INSERT INTO `spell_ranks` (`first_spell_id`, `spell_id`, `rank`) VALUES
(51505, 51505, 1),
(51505, 60043, 2);
DELETE FROM `spell_dbc` WHERE `ID` BETWEEN 910001 AND 910007;
