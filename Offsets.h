#pragma once

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <cstddef>
#include <string>
#include <algorithm>
#include <numeric>

using namespace Memory;

/* ===================================================== */
/* Mono / IL2CPP Base Structures */
/* ===================================================== */
struct monoArray {
    void* klass;
    void* obj;
    void* bounds;
    size_t max_length;
    uintptr_t m_Items;
};

struct monoList {
    void* klass;
    void* obj;
    uintptr_t monoArray;
    int pSize;
};

typedef struct Il2CppString {
    void* klass;
    void* obj;
    int32_t length;
    char chars[0];
} Il2CppString;

typedef unsigned short UTF16;

/* ===================================================== */
/* Mono Dictionary Structures */
/* ===================================================== */
template<typename TKey, typename TValue>
struct monoDictionaryEntry {
    int hashCode;
    int next;
    TKey key;
    TValue value;
};
template<typename TKey, typename TValue>
struct monoDictionary {
    void* klass;
    void* obj;
    int32_t* buckets;
    uintptr_t arrayEntries;
    int count;
    int version;
    int freeList;
    int freeCount;
    uintptr_t comparer;
    uintptr_t keys;
    uintptr_t values;
};

/* ===================================================== */
/* Helper Functions */
/* ===================================================== */
inline std::string getCharacterName(uintptr_t address) {
    int textLength = Read<int>(address + offsetof(Il2CppString, length));
    if (textLength <= 0) return std::string();

    UTF16* buf16 = new UTF16[textLength];
    for (int i = 0; i < textLength; i++) {
        int classname = Read<int>(address + offsetof(Il2CppString, chars) + i * 4);
        buf16[i * 2] = static_cast<UTF16>(classname & 0xffff);
        buf16[i * 2 + 1] = static_cast<UTF16>((classname & 0xfffff000) >> 16);
    }

    std::string utf8String;
    utf8::unchecked::utf16to8(buf16, buf16 + textLength, std::back_inserter(utf8String));
    delete[] buf16;
    return utf8String;
}

template<typename T>
inline std::vector<T> get_mono_list_vector(monoList array) {
    std::vector<T> output(array.pSize);
    if (array.monoArray == 0 || array.pSize <= 0) return output;

    auto address = array.monoArray + offsetof(monoArray, m_Items);
    for (int i = 0; i < array.pSize; i++) {
        output[i] = Memory::Read<T>(address + i * sizeof(T));
    }
    return output;
}

template<typename TKey, typename TValue>
inline std::vector<monoDictionaryEntry<TKey, TValue>> get_mono_dictionary_vector(monoDictionary<TKey, TValue> dict) {
    std::vector<monoDictionaryEntry<TKey, TValue>> output(dict.count);    if (dict.arrayEntries == 0 || dict.count <= 0) return output;

    auto address = dict.arrayEntries + offsetof(monoArray, m_Items);
    for (int i = 0; i < dict.count; i++) {
        output[i] = Memory::Read<monoDictionaryEntry<TKey, TValue>>(
            address + i * sizeof(monoDictionaryEntry<TKey, TValue>)
        );
    }
    return output;
}

/* ===================================================== */
/* Offsets Namespace */
/* ===================================================== */
namespace Offsets {

    #define OXOR_HEX(hex_str) [](){ return static_cast<size_t>(strtoul(hex_str, nullptr, 0)); }()

    /* ----------------------------------------------------- */
    /* BattleManager */
    /* ----------------------------------------------------- */
    namespace BattleManager {
        const size_t BasePtr         = OXOR_HEX("0x6F06DB8");
        const size_t LocalPlayerShow = OXOR_HEX("0x50");
        const size_t ShowPlayers     = OXOR_HEX("0x78");
        const size_t ShowMonsters    = OXOR_HEX("0x80");
        const size_t PtrChain1       = OXOR_HEX("0xB8");
        const size_t PtrChain2       = OXOR_HEX("0x08");
        const size_t ListDataOffset  = OXOR_HEX("0x10");
        const size_t ListCountOffset = OXOR_HEX("0x18");
        const size_t ListArrayOffset = OXOR_HEX("0x20");
    }

    /* ----------------------------------------------------- */
    /* ShowEntity (Base Class) */
    /* ----------------------------------------------------- */
    namespace ShowEntity {
        const size_t iType              = OXOR_HEX("0x80");
        const size_t Hp                 = OXOR_HEX("0x1ac");
        const size_t HpMax              = OXOR_HEX("0x1b0");
        const size_t bDeath             = OXOR_HEX("0xcd");
        const size_t bSameCampType      = OXOR_HEX("0x2b1");
        const size_t vCachePosition     = OXOR_HEX("0x294");
        const size_t HeroID             = OXOR_HEX("0x194");
        const size_t Level              = OXOR_HEX("0x198");
        const size_t KillWild           = OXOR_HEX("0xA60");
        const size_t m_killNum          = OXOR_HEX("0xa20");
        const size_t m_assistNum        = OXOR_HEX("0xa24");
        const size_t SummonSkillId      = OXOR_HEX("0x9a4");
        const size_t ShowEntityLayer    = OXOR_HEX("0x32c");        const size_t m_ShowCoolDownComp = OXOR_HEX("0x100");
    }

    /* ----------------------------------------------------- */
    /* ShowPlayer (Inherits ShowEntity) */
    /* ----------------------------------------------------- */
    namespace ShowPlayer {
        const size_t HeroName         = OXOR_HEX("0x468");
        const size_t m_EntityCampType = OXOR_HEX("0xd8");
        const size_t uiPlayerFov      = OXOR_HEX("0x41a");
    }

    /* ----------------------------------------------------- */
    /* Camera / Matrix */
    /* ----------------------------------------------------- */
    namespace Camera {
        const size_t MainCameraPtr = OXOR_HEX("0x6EF4430");
        const size_t CameraData    = OXOR_HEX("0xb8");
        const size_t CameraData2   = OXOR_HEX("0x08");
        const size_t SmoothFollow  = OXOR_HEX("0x10");
        const size_t ViewMatrix    = OXOR_HEX("0x5C");
    }

    /* ----------------------------------------------------- */
    /* String Helpers */
    /* ----------------------------------------------------- */
    namespace String {
        const size_t LengthOffset = OXOR_HEX("0x10");
        const size_t BufferOffset = OXOR_HEX("0x14");
    }

    /* ----------------------------------------------------- */
    /* SystemData */
    /* ----------------------------------------------------- */
    namespace SystemData {
        const size_t BasePtr          = OXOR_HEX("0x6f581a8");
        const size_t m_RoomPlayerInfo = OXOR_HEX("0x308");
        const size_t PtrChain1        = OXOR_HEX("0xB8");
        const size_t PtrChain2        = OXOR_HEX("0x08");
        const size_t m_uiID           = OXOR_HEX("0x318");
        const size_t m_rankLevelID           = OXOR_HEX("0x27c");
    }

    /* ----------------------------------------------------- */
    /* RoomData */
    /* ----------------------------------------------------- */
    namespace RoomData {
        const size_t bAutoConditionNew          = OXOR_HEX("0x10");
        const size_t bShowSeasonAchieve         = OXOR_HEX("0x11");
        const size_t iStyleBoardId              = OXOR_HEX("0x14");
        const size_t iMatchEffectId             = OXOR_HEX("0x18");        const size_t iDayBreakNo1Count          = OXOR_HEX("0x1c");
        const size_t lUid                       = OXOR_HEX("0x20");
        const size_t bUid                       = OXOR_HEX("0x28");
        const size_t iCamp                      = OXOR_HEX("0x30");
        const size_t iPos                       = OXOR_HEX("0x34");
        const size_t bAutoReadySelect           = OXOR_HEX("0x38");
        const size_t _sName                     = OXOR_HEX("0x40");
        const size_t bRobot                     = OXOR_HEX("0x48");
        const size_t heroid                     = OXOR_HEX("0x4c");
        const size_t heroskin                   = OXOR_HEX("0x50");
        const size_t headID                     = OXOR_HEX("0x54");
        const size_t uiSex                      = OXOR_HEX("0x58");
        const size_t country                    = OXOR_HEX("0x5c");
        const size_t uiZoneId                   = OXOR_HEX("0x60");
        const size_t summonSkillId              = OXOR_HEX("0x64");
        const size_t runeId                     = OXOR_HEX("0x68");
        const size_t mapTalentTree              = OXOR_HEX("0x70");
        const size_t mRuneSkill2023             = OXOR_HEX("0x78");
        const size_t runeLv                     = OXOR_HEX("0x80");
        const size_t skinlist                   = OXOR_HEX("0x88");
        const size_t facePath                   = OXOR_HEX("0x90");
        const size_t faceBorder                 = OXOR_HEX("0x98");
        const size_t bStarVip                   = OXOR_HEX("0x9c");
        const size_t bMCStarVip                 = OXOR_HEX("0x9d");
        const size_t bMCStarVipPlus             = OXOR_HEX("0x9e");
        const size_t ulRoomID                   = OXOR_HEX("0xa0");
        const size_t iConBlackRoomId            = OXOR_HEX("0xa8");
        const size_t banHero                    = OXOR_HEX("0xb0");
        const size_t vCanSelectHero             = OXOR_HEX("0xb8");
        const size_t vCanPickHero               = OXOR_HEX("0xc0");
        const size_t uiBattlePlayerType         = OXOR_HEX("0xc8");
        const size_t sThisLoginCountry          = OXOR_HEX("0xd0");
        const size_t sCreateRoleCountry         = OXOR_HEX("0xd8");
        const size_t uiLanguage                 = OXOR_HEX("0xe0");
        const size_t bIsOpenLive                = OXOR_HEX("0xe4");
        const size_t iTeamId                    = OXOR_HEX("0xe8");
        const size_t iTeamNationId              = OXOR_HEX("0xf0");
        const size_t _steamName                 = OXOR_HEX("0xf8");
        const size_t _steamSimpleName           = OXOR_HEX("0x100");
        const size_t iCertify                   = OXOR_HEX("0x108");
        const size_t lsEffectSkins              = OXOR_HEX("0x110");
        const size_t lsComEffSkins              = OXOR_HEX("0x118");
        const size_t vMissions                  = OXOR_HEX("0x120");
        const size_t uiRankLevel                = OXOR_HEX("0x128");
        const size_t uiPVPRank                  = OXOR_HEX("0x12c");
        const size_t bRankReview                = OXOR_HEX("0x130");
        const size_t iElo                       = OXOR_HEX("0x134");
        const size_t uiRoleLevel                = OXOR_HEX("0x138");
        const size_t bNewPlayer                 = OXOR_HEX("0x13c");
        const size_t iRoad                      = OXOR_HEX("0x140");        const size_t uiSkinSource               = OXOR_HEX("0x144");
        const size_t iFighterType               = OXOR_HEX("0x148");
        const size_t iWorldCupSupportCountry    = OXOR_HEX("0x14c");
        const size_t iHeroLevel                 = OXOR_HEX("0x150");
        const size_t iHeroSubLevel              = OXOR_HEX("0x154");
        const size_t iHeroPowerLevel            = OXOR_HEX("0x158");
        const size_t iActCamp                   = OXOR_HEX("0x15c");
        const size_t vTitle                     = OXOR_HEX("0x160");
        const size_t mHeroMission               = OXOR_HEX("0x168");
        const size_t vEmoji                     = OXOR_HEX("0x170");
        const size_t vItemBuff                  = OXOR_HEX("0x178");
        const size_t vMapPaint                  = OXOR_HEX("0x180");
        const size_t mSkinPaint                 = OXOR_HEX("0x188");
        const size_t sClientVersion             = OXOR_HEX("0x190");
        const size_t uiHolyStatue               = OXOR_HEX("0x198");
        const size_t uiKamon                    = OXOR_HEX("0x19c");
        const size_t uiUserMapID                = OXOR_HEX("0x1a0");
        const size_t iSurviveRank               = OXOR_HEX("0x1a4");
        const size_t iDefenceRankID             = OXOR_HEX("0x1a8");
        const size_t iLeagueWCNum               = OXOR_HEX("0x1ac");
        const size_t iLeagueFCNum               = OXOR_HEX("0x1b0");
        const size_t iMPLCertifyTime            = OXOR_HEX("0x1b4");
        const size_t iMPLCertifyID              = OXOR_HEX("0x1b8");
        const size_t mapBattleAttr              = OXOR_HEX("0x1c0");
        const size_t iHeroUseCount              = OXOR_HEX("0x1c8");
        const size_t iMythPoint                 = OXOR_HEX("0x1cc");
        const size_t bMythEvaled                = OXOR_HEX("0x1d0");
        const size_t iDefenceFlag               = OXOR_HEX("0x1d4");
        const size_t iDefenPoint                = OXOR_HEX("0x1d8");
        const size_t iDefenceMap                = OXOR_HEX("0x1dc");
        const size_t iAIType                    = OXOR_HEX("0x1e0");
        const size_t iAISeed                    = OXOR_HEX("0x1e4");
        const size_t sAiName                    = OXOR_HEX("0x1e8");
        const size_t iWarmValue                 = OXOR_HEX("0x1f0");
        const size_t uiAircraftIDChooose        = OXOR_HEX("0x1f4");
        const size_t uiHeroIDChoose             = OXOR_HEX("0x1f8");
        const size_t uiHeroSkinIDChoose         = OXOR_HEX("0x1fc");
        const size_t uiMapIDChoose              = OXOR_HEX("0x200");
        const size_t uiMapSkinIDChoose          = OXOR_HEX("0x204");
        const size_t uiDefenceRankScore         = OXOR_HEX("0x208");
        const size_t bBanChat                   = OXOR_HEX("0x20c");
        const size_t iChatBanFinishTime         = OXOR_HEX("0x210");
        const size_t iChatBanBattleNum          = OXOR_HEX("0x214");
        const size_t vFastChat                  = OXOR_HEX("0x218");
        const size_t vWantSelectHero            = OXOR_HEX("0x220");
        const size_t mapHeroCareerNumInfo       = OXOR_HEX("0x228");
        const size_t vRecentRoadInfo            = OXOR_HEX("0x230");
        const size_t bTeamMCLChampion           = OXOR_HEX("0x238");
        const size_t vUnLockHeroInRank          = OXOR_HEX("0x240");
        const size_t uiEquipSuit                = OXOR_HEX("0x248");        const size_t vOperBanHero               = OXOR_HEX("0x250");
        const size_t iHeroEnhanceLevel          = OXOR_HEX("0x258");
        const size_t uiMagicChessCupNum         = OXOR_HEX("0x25c");
        const size_t uiMagicRankID              = OXOR_HEX("0x260");
        const size_t chessHeroSkin              = OXOR_HEX("0x268");
        const size_t mChessHeroSkin2            = OXOR_HEX("0x270");
        const size_t uiAerocraftId              = OXOR_HEX("0x278");
        const size_t uiAerocraftSkinId          = OXOR_HEX("0x27c");
        const size_t uiCupCount                 = OXOR_HEX("0x280");
        const size_t uiBattleCount              = OXOR_HEX("0x284");
        const size_t heroShowSkin               = OXOR_HEX("0x288");
        const size_t uiMatchScore               = OXOR_HEX("0x28c");
        const size_t iStarVipSkinRank           = OXOR_HEX("0x290");
        const size_t vSkinAction                = OXOR_HEX("0x298");
        const size_t uiBattleAIPerformanceScore = OXOR_HEX("0x2a0");
        const size_t uiLocalAIPerformanceScore  = OXOR_HEX("0x2a4");
        const size_t uiTemperatureGrade         = OXOR_HEX("0x2a8");
        const size_t mBattleAILimitInfos        = OXOR_HEX("0x2b0");
        const size_t mAutoEmojiInfos            = OXOR_HEX("0x2b8");
        const size_t bBattleAIWhilteList        = OXOR_HEX("0x2c0");
        const size_t bOpenTryRune               = OXOR_HEX("0x2c1");
        const size_t uiBattleAIDiffculty        = OXOR_HEX("0x2c4");
        const size_t uiBattleAIDiffcultyMin     = OXOR_HEX("0x2c8");
        const size_t uiBattleAIDiffcultyMax     = OXOR_HEX("0x2cc");
        const size_t bWarmBattleAIRobot         = OXOR_HEX("0x2d0");
        const size_t uiRobotAICompany           = OXOR_HEX("0x2d4");
        const size_t uiPlayerFov                = OXOR_HEX("0x2d8");
        const size_t uiCameraProjection         = OXOR_HEX("0x2dc");
        const size_t uiArenaRankID              = OXOR_HEX("0x2e0");
        const size_t uiArenaCupNum              = OXOR_HEX("0x2e4");
        const size_t bFaceHD                    = OXOR_HEX("0x2e8");
        const size_t bLuckyDogFreeHero          = OXOR_HEX("0x2e9");
        const size_t bLuckyDogFreeSkin          = OXOR_HEX("0x2ea");
        const size_t iHistoryMaxBigRankId       = OXOR_HEX("0x2ec");
        const size_t mCommanderSkinAnimationChooose = OXOR_HEX("0x2f0");
        const size_t mMagicChessInteractiveGift = OXOR_HEX("0x2f8");
        const size_t mArenaCard                 = OXOR_HEX("0x300");
        const size_t uiArenaBigRankID           = OXOR_HEX("0x308");
        const size_t bDropSpecialItemInDayBreak = OXOR_HEX("0x30c");
        const size_t stCompanyBattleAI          = OXOR_HEX("0x310");
        const size_t bFakeRole                  = OXOR_HEX("0x318");
        const size_t bVP                        = OXOR_HEX("0x319");
        const size_t uiCloneImbaDisorderSpecialResource = OXOR_HEX("0x31c");
        const size_t mFriendIntimacy            = OXOR_HEX("0x320");
        const size_t vFriendShareHero           = OXOR_HEX("0x328");
        const size_t vPartyEffect               = OXOR_HEX("0x330");
        const size_t bInspire                   = OXOR_HEX("0x338");
        const size_t uiRandomScroeItem          = OXOR_HEX("0x33c");
        const size_t iPartyTitle                = OXOR_HEX("0x340");
        const size_t bRoomLeader                = OXOR_HEX("0x344");        const size_t vExtraSummons              = OXOR_HEX("0x348");
        const size_t newRuneAttr                = OXOR_HEX("0x350");
        const size_t bForbidUseFaceName         = OXOR_HEX("0x358");
        const size_t sClientIp                  = OXOR_HEX("0x360");
        const size_t iRoomOrder                 = OXOR_HEX("0x368");
        const size_t vRougeTotalSkill           = OXOR_HEX("0x370");
        const size_t vRougeOMGSkill             = OXOR_HEX("0x378");
        const size_t vRecommendEquipList        = OXOR_HEX("0x380");
        const size_t sRecommendEquipVersion     = OXOR_HEX("0x388");
        const size_t vPingParamDetail           = OXOR_HEX("0x390");
        const size_t uiPlayerPing               = OXOR_HEX("0x398");
        const size_t mSkinRankSeasonTag         = OXOR_HEX("0x3a0");
        const size_t mSkinNumTag                = OXOR_HEX("0x3a8");
        const size_t bFullSkillaber             = OXOR_HEX("0x3b0");
        const size_t uiCommanderSkinAttackEffect = OXOR_HEX("0x3b4");
        const size_t uiDailyFreeRandomNum       = OXOR_HEX("0x3b8");
        const size_t bIllustrateCornerEffectClose = OXOR_HEX("0x3bc");
        const size_t bTagedBackOf2022           = OXOR_HEX("0x3bd");
        const size_t iTapConflictTipNum         = OXOR_HEX("0x3c0");
        const size_t iNameShowType              = OXOR_HEX("0x3c4");
        const size_t bOpenHighLight             = OXOR_HEX("0x3c8");
        const size_t mMCBanPickCommander        = OXOR_HEX("0x3d0");
        const size_t vForbidBanCommander        = OXOR_HEX("0x3d8");
        const size_t iTeamLevel                 = OXOR_HEX("0x3e0");
        const size_t vAdditionalHero            = OXOR_HEX("0x3e8");
        const size_t uiDisorderPublicHeroScore  = OXOR_HEX("0x3f0");
        const size_t bPlayerBirthdayToday       = OXOR_HEX("0x3f4");
        const size_t iTeamHeadId                = OXOR_HEX("0x3f8");
        const size_t mapHeroBattleNum           = OXOR_HEX("0x400");
        const size_t vCurSeasonRealRoadInfo     = OXOR_HEX("0x408");
        const size_t vCultivateRoadShow         = OXOR_HEX("0x410");
        const size_t uiCommanderLevel           = OXOR_HEX("0x418");
        const size_t bOpenSubRankID             = OXOR_HEX("0x41c");
        const size_t iSubRankID                 = OXOR_HEX("0x420");
        const size_t iSingleLv                  = OXOR_HEX("0x424");
        const size_t stArenaMatchBattleInfo     = OXOR_HEX("0x428");
        const size_t stArenaMatchShowInfo       = OXOR_HEX("0x430");
        const size_t stSkinAttach               = OXOR_HEX("0x438");
        const size_t iMatchTeamId               = OXOR_HEX("0x440");
        const size_t iFlowBackTYpe              = OXOR_HEX("0x448");
        const size_t bRoadAdditionCover         = OXOR_HEX("0x44c");
        const size_t iRoadAdditionCoverTimes    = OXOR_HEX("0x450");
        const size_t iRoomPos                   = OXOR_HEX("0x454");
        const size_t stEasterEggInfo            = OXOR_HEX("0x458");
        const size_t sMatchTeamName             = OXOR_HEX("0x460");
        const size_t iMatchTeamFaceId           = OXOR_HEX("0x468");
        const size_t iMatchPhaseId              = OXOR_HEX("0x46c");
        const size_t stLaneMatch                = OXOR_HEX("0x470");
        const size_t vAllPlayerTitle            = OXOR_HEX("0x478");
        const size_t iTeamMatchBONum            = OXOR_HEX("0x480");        const size_t iTeamMatchRank             = OXOR_HEX("0x484");
        const size_t stMCTreeAILineup           = OXOR_HEX("0x488");
        const size_t dragonCrystalId            = OXOR_HEX("0x490");
        const size_t iBoolHolder                = OXOR_HEX("0x498");
        const size_t iRoguelikeTeamID           = OXOR_HEX("0x4a0");
        const size_t stHonorAbout               = OXOR_HEX("0x4a8");
        const size_t stBattleChuoChuoInfo       = OXOR_HEX("0x4b0");
        const size_t iChatEffectId              = OXOR_HEX("0x4b8");
        const size_t iMirrorBattleID            = OXOR_HEX("0x4c0");
        const size_t vFiveFreeSkin              = OXOR_HEX("0x4c8");
        const size_t uiCurtainId                = OXOR_HEX("0x4d0");
        const size_t ulFakeRoleRoomID           = OXOR_HEX("0x4d8");
        const size_t vDisOrderUnlockHero        = OXOR_HEX("0x4e0");
        const size_t stNameDisplay              = OXOR_HEX("0x4e8");
        const size_t iForbidBattleMessageTime   = OXOR_HEX("0x4f0");
        const size_t stBattleCollectionSkinInfo = OXOR_HEX("0x4f8");
        const size_t iButtonSkin                = OXOR_HEX("0x500");
        const size_t vRecommendEquip            = OXOR_HEX("0x508");
        const size_t vFlowBack2023RecommendHeroList = OXOR_HEX("0x510");
        const size_t stSeasonHeroPoolBattleInfo = OXOR_HEX("0x518");
        const size_t bNeedAITeacher             = OXOR_HEX("0x520");
        const size_t stBattlePlayerMode         = OXOR_HEX("0x528");
    }

    /* ----------------------------------------------------- */
    /* UIRegion */
    /* ----------------------------------------------------- */
    namespace UIRegion {
        const size_t BasePtr         = OXOR_HEX("0x6e4f570");
        const size_t PtrChain1       = OXOR_HEX("0xB8");
        const size_t LoginServerInfo = OXOR_HEX("0x1b0");
    }

    /* ----------------------------------------------------- */
    /* LogicBattleManager */
    /* ----------------------------------------------------- */
    namespace LogicBattleManager {
        const size_t BasePtr          = OXOR_HEX("0x6F60B98");
        const size_t PtrChain1        = OXOR_HEX("0xB8");
        const size_t m_uiFrameTime    = OXOR_HEX("0x19c");
        const size_t m_dicPlayerLogic = OXOR_HEX("0xa8");
        const size_t _m_eState        = OXOR_HEX("0x180");
    }

    /* ----------------------------------------------------- */
    /* ShowCoolDownComp */
    /* ----------------------------------------------------- */
    namespace ShowCoolDownComp {
        const size_t _dicCD = OXOR_HEX("0x18");
    }
    /* ----------------------------------------------------- */
    /* CoolDownData */
    /* ----------------------------------------------------- */
    namespace CoolDownData {
        const size_t iSpellID            = OXOR_HEX("0x10");
        const size_t uiCoolTime          = OXOR_HEX("0x14");
        const size_t originalMaxCdTime   = OXOR_HEX("0x18");
        const size_t uiStartTime         = OXOR_HEX("0x1C");
        const size_t m_isCoolDown        = OXOR_HEX("0x20");
        const size_t iNewCoolChangeStep  = OXOR_HEX("0x24");
    }

    /* ===================================================== */
    /* Macro Helpers */
    /* ===================================================== */
    #define OFF_BM(field)  Offsets::BattleManager::field
    #define OFF_SE(field)  Offsets::ShowEntity::field
    #define OFF_SP(field)  Offsets::ShowPlayer::field
    #define OFF_CAM(field) Offsets::Camera::field
    #define OFF_STR(field) Offsets::String::field
    #define OFF_SD(field)  Offsets::SystemData::field
    #define OFF_RD(field)  Offsets::RoomData::field
    #define OFF_UIREG(field) Offsets::UIRegion::field
    #define OFF_BLC(field) Offsets::LogicBattleManager::field
    #define OFF_SCC(field) Offsets::ShowCoolDownComp::field
    #define OFF_CDD(field) Offsets::CoolDownData::field

} /* namespace Offsets */