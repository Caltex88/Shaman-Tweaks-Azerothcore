/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>
 * Module: mod-shaman-tweaks
 *
 * Config-driven shaman changes:
 *   - Instant Ghost Wolf
 *   - Lava Burst refreshes the caster's Flame Shock DoT
 *   - Extra Lava Burst ranks (or level-scaled fallback)
 */

#include "Config.h"
#include "DBCStores.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "Unit.h"
#include "Util.h"

#include <array>

namespace
{
    // Vanilla
    uint32 constexpr SPELL_GHOST_WOLF           = 2645;
    uint32 constexpr SPELL_LAVA_BURST_R8        = 51505; // vanilla rank 1, remapped to rank 8
    uint32 constexpr SPELL_LAVA_BURST_R9        = 60043; // vanilla rank 2, remapped to rank 9

    // Custom ranks 1-7 (created via module SQL spell_dbc)
    uint32 constexpr SPELL_LAVA_BURST_R1        = 910001;
    uint32 constexpr SPELL_LAVA_BURST_R2        = 910002;
    uint32 constexpr SPELL_LAVA_BURST_R3        = 910003;
    uint32 constexpr SPELL_LAVA_BURST_R4        = 910004;
    uint32 constexpr SPELL_LAVA_BURST_R5        = 910005;
    uint32 constexpr SPELL_LAVA_BURST_R6        = 910006;
    uint32 constexpr SPELL_LAVA_BURST_R7        = 910007;

    // Flame Shock SpellFamilyFlags[0]
    uint32 constexpr FLAME_SHOCK_FAMILY_FLAGS0  = 0x10000000;

    // Vanilla 60043 damage (to restore when the option is turned off)
    int32  constexpr VANILLA_R9_BASEPOINTS      = 1191; // 1192-1518
    int32  constexpr VANILLA_R9_DIESIDES        = 327;

    struct LavaBurstRank
    {
        uint32 spellId;
        uint8  level;
        uint32 minDamage;
        uint32 maxDamage;
    };

    // Rank 1-7 are custom IDs. Rank 8/9 reuse vanilla spells.
    std::array<LavaBurstRank, 9> const LavaBurstRanks = {{
        { SPELL_LAVA_BURST_R1, 10,   81,   95 },
        { SPELL_LAVA_BURST_R2, 20,  150,  171 },
        { SPELL_LAVA_BURST_R3, 30,  309,  350 },
        { SPELL_LAVA_BURST_R4, 40,  408,  460 },
        { SPELL_LAVA_BURST_R5, 50,  624,  700 },
        { SPELL_LAVA_BURST_R6, 60,  891, 1017 },
        { SPELL_LAVA_BURST_R7, 70, 1015, 1160 },
        { SPELL_LAVA_BURST_R8, 75, 1012, 1290 },
        { SPELL_LAVA_BURST_R9, 80, 1200, 1550 },
    }};

    bool ConfEnabled()
    {
        return sConfigMgr->GetOption<bool>("ShamanTweaks.Enable", true);
    }

    bool ConfGhostWolfInstant()
    {
        return ConfEnabled() && sConfigMgr->GetOption<bool>("ShamanTweaks.GhostWolfInstant", true);
    }

    bool ConfRefreshFlameShock()
    {
        return ConfEnabled() && sConfigMgr->GetOption<bool>("ShamanTweaks.LavaBurstRefreshFlameShock", true);
    }

    bool ConfExtraRanks()
    {
        return ConfEnabled() && sConfigMgr->GetOption<bool>("ShamanTweaks.LavaBurstExtraRanks", true);
    }

    bool ConfAutoLearn()
    {
        return ConfExtraRanks() && sConfigMgr->GetOption<bool>("ShamanTweaks.AutoLearnRanks", true);
    }

    bool CustomRanksLoaded()
    {
        return sSpellMgr->GetSpellInfo(SPELL_LAVA_BURST_R1) != nullptr;
    }

    bool IsLavaBurstSpell(uint32 spellId)
    {
        switch (spellId)
        {
            case SPELL_LAVA_BURST_R1:
            case SPELL_LAVA_BURST_R2:
            case SPELL_LAVA_BURST_R3:
            case SPELL_LAVA_BURST_R4:
            case SPELL_LAVA_BURST_R5:
            case SPELL_LAVA_BURST_R6:
            case SPELL_LAVA_BURST_R7:
            case SPELL_LAVA_BURST_R8:
            case SPELL_LAVA_BURST_R9:
                return true;
            default:
                break;
        }

        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId))
            return info->SpellFamilyName == SPELLFAMILY_SHAMAN && (info->SpellFamilyFlags[1] & 0x00001000);

        return false;
    }

    LavaBurstRank const* RankForLevel(uint8 level)
    {
        LavaBurstRank const* best = nullptr;
        for (LavaBurstRank const& rank : LavaBurstRanks)
        {
            if (level >= rank.level)
                best = &rank;
        }
        return best;
    }

    void SetEffectDamage(SpellInfo* spell, uint32 minDamage, uint32 maxDamage)
    {
        // WoW damage = (BasePoints + 1) .. (BasePoints + DieSides)
        spell->Effects[EFFECT_0].BasePoints = int32(minDamage) - 1;
        spell->Effects[EFFECT_0].DieSides   = int32(maxDamage - minDamage + 1);
    }

    void CopyLavaBurstTemplate(SpellInfo* dest, SpellInfo const* src, LavaBurstRank const& rank)
    {
        dest->Attributes              = src->Attributes;
        dest->AttributesEx            = src->AttributesEx;
        dest->AttributesEx2           = src->AttributesEx2;
        dest->AttributesEx3           = src->AttributesEx3;
        dest->AttributesEx4           = src->AttributesEx4;
        dest->AttributesEx5           = src->AttributesEx5;
        dest->AttributesEx6           = src->AttributesEx6;
        dest->AttributesEx7           = src->AttributesEx7;
        dest->AttributesCu            = src->AttributesCu;
        dest->Targets                 = src->Targets;
        dest->FacingCasterFlags       = src->FacingCasterFlags;
        dest->CastTimeEntry           = src->CastTimeEntry;
        dest->RecoveryTime            = src->RecoveryTime;
        dest->CategoryRecoveryTime    = src->CategoryRecoveryTime;
        dest->StartRecoveryCategory   = src->StartRecoveryCategory;
        dest->StartRecoveryTime       = src->StartRecoveryTime;
        dest->InterruptFlags          = src->InterruptFlags;
        dest->AuraInterruptFlags      = src->AuraInterruptFlags;
        dest->ChannelInterruptFlags   = src->ChannelInterruptFlags;
        dest->ProcFlags               = src->ProcFlags;
        dest->ProcChance              = src->ProcChance;
        dest->MaxLevel                = src->MaxLevel;
        dest->BaseLevel               = rank.level;
        dest->SpellLevel              = rank.level;
        dest->DurationEntry           = src->DurationEntry;
        dest->PowerType               = src->PowerType;
        dest->ManaCost                = src->ManaCost;
        dest->ManaCostPerlevel        = src->ManaCostPerlevel;
        dest->ManaPerSecond           = src->ManaPerSecond;
        dest->ManaPerSecondPerLevel   = src->ManaPerSecondPerLevel;
        dest->ManaCostPercentage      = 10;
        dest->RangeEntry              = src->RangeEntry;
        dest->Speed                   = src->Speed;
        dest->SpellVisual             = src->SpellVisual;
        dest->SpellIconID             = src->SpellIconID;
        dest->ActiveIconID            = src->ActiveIconID;
        dest->SpellName               = src->SpellName;
        dest->SpellFamilyName         = src->SpellFamilyName;
        dest->SpellFamilyFlags        = src->SpellFamilyFlags;
        dest->DmgClass                = src->DmgClass;
        dest->PreventionType          = src->PreventionType;
        dest->SchoolMask              = src->SchoolMask;
        dest->ExplicitTargetMask      = src->ExplicitTargetMask;
        dest->CategoryEntry           = src->CategoryEntry;

        dest->Effects[EFFECT_0].BonusMultiplier   = src->Effects[EFFECT_0].BonusMultiplier;
        dest->Effects[EFFECT_0].DamageMultiplier  = src->Effects[EFFECT_0].DamageMultiplier;
        dest->Effects[EFFECT_0].ValueMultiplier   = src->Effects[EFFECT_0].ValueMultiplier;
        dest->Effects[EFFECT_0].RealPointsPerLevel = 0.0f;
        SetEffectDamage(dest, rank.minDamage, rank.maxDamage);

        dest->SetCritCapable(true);
        dest->SetSpellValid(true);
        dest->_InitializeExplicitTargetMask();
    }

    SpellCastTimesEntry const* s_originalGhostWolfCastTime = nullptr;
    int32 s_originalR9BasePoints = VANILLA_R9_BASEPOINTS;
    int32 s_originalR9DieSides   = VANILLA_R9_DIESIDES;
    bool  s_capturedOriginals    = false;

    void CaptureOriginalsOnce()
    {
        if (s_capturedOriginals)
            return;

        if (SpellInfo const* gw = sSpellMgr->GetSpellInfo(SPELL_GHOST_WOLF))
            s_originalGhostWolfCastTime = gw->CastTimeEntry;

        if (SpellInfo const* r9 = sSpellMgr->GetSpellInfo(SPELL_LAVA_BURST_R9))
        {
            s_originalR9BasePoints = r9->Effects[EFFECT_0].BasePoints;
            s_originalR9DieSides   = r9->Effects[EFFECT_0].DieSides;
        }

        s_capturedOriginals = true;
    }

    void ApplySpellInfoTweaks()
    {
        CaptureOriginalsOnce();

        if (SpellInfo* ghostWolf = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(SPELL_GHOST_WOLF)))
        {
            if (ConfGhostWolfInstant())
            {
                if (SpellCastTimesEntry const* instant = sSpellCastTimesStore.LookupEntry(1))
                    ghostWolf->CastTimeEntry = instant;
            }
            else
                ghostWolf->CastTimeEntry = s_originalGhostWolfCastTime;
        }

        SpellInfo const* templateInfo = sSpellMgr->GetSpellInfo(SPELL_LAVA_BURST_R8);
        if (!templateInfo)
            return;

        if (ConfExtraRanks())
        {
            if (CustomRanksLoaded())
            {
                for (uint8 i = 0; i < 7; ++i)
                {
                    if (SpellInfo* dest = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(LavaBurstRanks[i].spellId)))
                        CopyLavaBurstTemplate(dest, templateInfo, LavaBurstRanks[i]);
                }
            }

            if (SpellInfo* r9 = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(SPELL_LAVA_BURST_R9)))
                SetEffectDamage(r9, 1200, 1550);
        }
        else if (SpellInfo* r9 = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(SPELL_LAVA_BURST_R9)))
        {
            r9->Effects[EFFECT_0].BasePoints = s_originalR9BasePoints;
            r9->Effects[EFFECT_0].DieSides   = s_originalR9DieSides;
        }
    }

    void LearnIfMissing(Player* player, uint32 spellId)
    {
        if (!sSpellMgr->GetSpellInfo(spellId))
            return;

        if (!player->HasSpell(spellId))
            player->learnSpell(spellId);
    }

    void UnlearnIfKnown(Player* player, uint32 spellId)
    {
        if (player->HasSpell(spellId))
            player->removeSpell(spellId, SPEC_MASK_ALL, false);
    }

    void SyncLavaBurstRanks(Player* player)
    {
        if (!player || player->getClass() != CLASS_SHAMAN)
            return;

        if (!ConfAutoLearn())
            return;

        uint8 const level = player->GetLevel();

        if (!ConfExtraRanks())
        {
            for (uint8 i = 0; i < 7; ++i)
                UnlearnIfKnown(player, LavaBurstRanks[i].spellId);

            if (level >= 80)
                LearnIfMissing(player, SPELL_LAVA_BURST_R9);
            else
                UnlearnIfKnown(player, SPELL_LAVA_BURST_R9);

            if (level >= 75)
                LearnIfMissing(player, SPELL_LAVA_BURST_R8);
            else
                UnlearnIfKnown(player, SPELL_LAVA_BURST_R8);

            return;
        }

        if (CustomRanksLoaded())
        {
            LavaBurstRank const* best = RankForLevel(level);
            if (!best)
                return;

            // Unlearn every other rank so the spellbook holds a single upgrade.
            for (LavaBurstRank const& rank : LavaBurstRanks)
            {
                if (rank.spellId != best->spellId)
                    UnlearnIfKnown(player, rank.spellId);
            }

            LearnIfMissing(player, best->spellId);
            return;
        }

        // Fallback: vanilla 51505 at 10 (damage scaled in the spell script). Rank 9 at 80.
        if (level >= 80)
        {
            LearnIfMissing(player, SPELL_LAVA_BURST_R9);
            UnlearnIfKnown(player, SPELL_LAVA_BURST_R8);
        }
        else if (level >= 10)
            LearnIfMissing(player, SPELL_LAVA_BURST_R8);
    }

    void RefreshCasterFlameShock(Unit* caster, Unit* target)
    {
        if (!caster || !target)
            return;

        Unit::AuraEffectList const& dots = target->GetAuraEffectsByType(SPELL_AURA_PERIODIC_DAMAGE);
        for (AuraEffect const* aurEff : dots)
        {
            if (!aurEff || aurEff->GetCasterGUID() != caster->GetGUID())
                continue;

            SpellInfo const* info = aurEff->GetSpellInfo();
            if (!info)
                continue;

            if (info->SpellFamilyName == SPELLFAMILY_SHAMAN && (info->SpellFamilyFlags[0] & FLAME_SHOCK_FAMILY_FLAGS0))
            {
                if (Aura* aura = aurEff->GetBase())
                    aura->RefreshDuration();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Spell script: Lava Burst hit + optional fallback damage scaling
// ---------------------------------------------------------------------------
class spell_sha_lava_burst_tweaks : public SpellScript
{
    PrepareSpellScript(spell_sha_lava_burst_tweaks);

    void HandleLaunchTarget(SpellEffIndex /*effIndex*/)
    {
        if (!ConfExtraRanks() || CustomRanksLoaded())
            return;

        // Fallback path: scale vanilla 51505 to the rank table by caster level.
        if (GetSpellInfo()->Id != SPELL_LAVA_BURST_R8)
            return;

        Unit* caster = GetCaster();
        if (!caster || !caster->IsPlayer())
            return;

        LavaBurstRank const* rank = RankForLevel(caster->ToPlayer()->GetLevel());
        if (!rank)
            return;

        SetEffectValue(int32(urand(rank->minDamage, rank->maxDamage)));
    }

    void HandleAfterHit()
    {
        if (!ConfRefreshFlameShock())
            return;

        RefreshCasterFlameShock(GetCaster(), GetHitUnit());
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_sha_lava_burst_tweaks::HandleLaunchTarget, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        AfterHit += SpellHitFn(spell_sha_lava_burst_tweaks::HandleAfterHit);
    }
};

// ---------------------------------------------------------------------------
// Apply SpellInfo changes as the core finishes loading each spell
// ---------------------------------------------------------------------------
class ShamanTweaksGlobalScript : public GlobalScript
{
public:
    ShamanTweaksGlobalScript() : GlobalScript("ShamanTweaksGlobalScript", {
        GLOBALHOOK_ON_LOAD_SPELL_CUSTOM_ATTR
    }) { }

    void OnLoadSpellCustomAttr(SpellInfo* spell) override
    {
        if (!spell)
            return;

        if (spell->Id == SPELL_GHOST_WOLF)
        {
            CaptureOriginalsOnce();
            if (ConfGhostWolfInstant())
                if (SpellCastTimesEntry const* instant = sSpellCastTimesStore.LookupEntry(1))
                    spell->CastTimeEntry = instant;
        }

        if (!ConfExtraRanks())
            return;

        SpellInfo const* templateInfo = sSpellMgr->GetSpellInfo(SPELL_LAVA_BURST_R8);
        if (!templateInfo)
            return;

        for (uint8 i = 0; i < 7; ++i)
        {
            if (spell->Id == LavaBurstRanks[i].spellId)
            {
                CopyLavaBurstTemplate(spell, templateInfo, LavaBurstRanks[i]);
                return;
            }
        }

        if (spell->Id == SPELL_LAVA_BURST_R9)
        {
            CaptureOriginalsOnce();
            SetEffectDamage(spell, 1200, 1550);
        }
    }
};

// ---------------------------------------------------------------------------
// Config load / startup
// ---------------------------------------------------------------------------
class ShamanTweaksWorldScript : public WorldScript
{
public:
    ShamanTweaksWorldScript() : WorldScript("ShamanTweaksWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_STARTUP
    }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload)
            ApplySpellInfoTweaks();
    }

    void OnStartup() override
    {
        ApplySpellInfoTweaks();

        if (!ConfEnabled())
        {
            LOG_INFO("module", "Shaman Tweaks: disabled (ShamanTweaks.Enable = 0)");
            return;
        }

        LOG_INFO("module", "Shaman Tweaks: loaded (GhostWolfInstant={}, RefreshFlameShock={}, ExtraRanks={}, CustomRankSpells={})",
            ConfGhostWolfInstant() ? 1 : 0,
            ConfRefreshFlameShock() ? 1 : 0,
            ConfExtraRanks() ? 1 : 0,
            CustomRanksLoaded() ? 1 : 0);

        if (ConfExtraRanks() && !CustomRanksLoaded())
            LOG_INFO("module", "Shaman Tweaks: custom Lava Burst IDs 910001-910007 were not found. Using level-scaled 51505 fallback.");
    }
};

// ---------------------------------------------------------------------------
// Auto-learn ranks for players and playerbots
// ---------------------------------------------------------------------------
class ShamanTweaksPlayerScript : public PlayerScript
{
public:
    ShamanTweaksPlayerScript() : PlayerScript("ShamanTweaksPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LEVEL_CHANGED
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        SyncLavaBurstRanks(player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        SyncLavaBurstRanks(player);
    }
};

void AddShamanTweaksScripts()
{
    new ShamanTweaksGlobalScript();
    new ShamanTweaksWorldScript();
    new ShamanTweaksPlayerScript();
    RegisterSpellScript(spell_sha_lava_burst_tweaks);
}
