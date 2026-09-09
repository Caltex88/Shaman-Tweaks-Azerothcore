-- Only run this if your world database still uses `npc_trainer`
-- instead of `trainer_spell` (older AzerothCore). Modern cores
-- including the playerbots fork already applied trainer_spell in
-- the main module SQL.

DELETE FROM `npc_trainer` WHERE `SpellID` BETWEEN 910001 AND 910007;

INSERT INTO `npc_trainer` (`ID`, `SpellID`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqLevel`, `ReqSpell`)
SELECT `ID`, 910001,    400, 0, 0, 10,      0 FROM `npc_trainer` WHERE `SpellID` = 51505;
INSERT INTO `npc_trainer` (`ID`, `SpellID`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqLevel`, `ReqSpell`)
SELECT `ID`, 910002,   1800, 0, 0, 20, 910001 FROM `npc_trainer` WHERE `SpellID` = 51505;
INSERT INTO `npc_trainer` (`ID`, `SpellID`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqLevel`, `ReqSpell`)
SELECT `ID`, 910003,  10000, 0, 0, 30, 910002 FROM `npc_trainer` WHERE `SpellID` = 51505;
INSERT INTO `npc_trainer` (`ID`, `SpellID`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqLevel`, `ReqSpell`)
SELECT `ID`, 910004,  18000, 0, 0, 40, 910003 FROM `npc_trainer` WHERE `SpellID` = 51505;
INSERT INTO `npc_trainer` (`ID`, `SpellID`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqLevel`, `ReqSpell`)
SELECT `ID`, 910005,  27000, 0, 0, 50, 910004 FROM `npc_trainer` WHERE `SpellID` = 51505;
INSERT INTO `npc_trainer` (`ID`, `SpellID`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqLevel`, `ReqSpell`)
SELECT `ID`, 910006,  34000, 0, 0, 60, 910005 FROM `npc_trainer` WHERE `SpellID` = 51505;
INSERT INTO `npc_trainer` (`ID`, `SpellID`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqLevel`, `ReqSpell`)
SELECT `ID`, 910007, 100000, 0, 0, 70, 910006 FROM `npc_trainer` WHERE `SpellID` = 51505;
