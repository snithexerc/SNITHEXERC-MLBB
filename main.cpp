#include "main.h"
#include <linux/input.h>
#include <linux/uinput.h>
#include <vector>
#include <functional>
#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <fstream>
#include <cstring>
#include <ctime>
#include <malloc.h> 
#include <iostream>
#include <sys/system_properties.h>
#include <string>
#include <sstream>
#include <exception>
#include <array>
#include <algorithm>
#include <cmath>
#include <thread>
#include <mutex>
#include <atomic>
#include "Memory/Memory.h"
#include "Memory/PatternScanner.h"
#include "Quaternion.hpp"
#include "Vector2.hpp"
#include "Vector3.hpp"
#include "Includes/Log.h"
#include "Includes/Offset.h"
#include "Engine/CanvasView.h"
#include "include.h"
#include "Matrix4x4.hpp"
#include "ToString.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "icon/HeroIcons.h"
#include "Decoder64.h"
#include "DrawIconHero.h"
#include "Offsets.h"
#include "SidebarIcons.h"

bool CHKEXP(){
int expYear   = 9026;
int expMonth  = 4;
int expDay    = 16;
int expHour   = 23;    int expMinute = 59;
int expSecond = 0;

std::time_t now = std::time(nullptr);
std::tm *nowTm = std::localtime(&now);

std::tm expTm = {};    expTm.tm_year = expYear - 1900;    expTm.tm_mon  = expMonth - 1;
expTm.tm_mday = expDay;
expTm.tm_hour = expHour;
expTm.tm_min  = expMinute;
expTm.tm_sec  = expSecond;
std::time_t expTime = std::mktime(&expTm);

if (std::difftime(now, expTime) >= 0)
{
return false;
}
return true;
}

using namespace Memory;
bool is_root_mode = true;

bool main_thread_flag = true;
int abs_ScreenX = 0;
int abs_ScreenY = 0;
bool drawMHealth = true;
bool drawMDistance = true;
bool drawAlertUnderAttack = true;
bool iconhero = true;
bool is_attached = false;

// ====== Drone View ======
bool  EnableDrone = false;
float FieldView   = -1.0f;
bool  EnableDroneV2 = false;
float DroneHeight   = 0.0f;   // Drone V2: Camera height (SmoothFollow v3.Y @ 0xD8)

long libbase = 0;

void AttachToGame() {
pid = pidof(oxorany("com.mobile.legends:UnityKillsMe"));
if (pid > 0) {
g_pid = pid;
libbase = GetBase(oxorany("libcsharp.so"));
if (libbase != 0) {
is_attached = true;
}
}
}
std::string fshy(uintptr_t address)
{
if (!address) return "";

uint32_t stringLength = Read<uint32_t>(address + OFF_STR(LengthOffset));
if (stringLength == 0) return "";

if (stringLength > 1024) stringLength = 1024;

std::vector<char16_t> buffer(stringLength + 1, 0);

uintptr_t stringPtr = address + OFF_STR(BufferOffset);
if (!stringPtr) return "";

if (!pvm(reinterpret_cast<void *>(stringPtr), buffer.data(), static_cast<size_t>(stringLength) * 2, false)) {
return "";
}

return utf16_to_utf8(buffer.data(), stringLength);
}

struct RoomPlayerInfo {
std::string Name;
std::string UserID;
std::string Squad;
std::string Rank;
std::string Hero;
std::string Spell;
int  SpellID = 0;   /* Spell icon ID for texture lookup */
bool isBot   = false; /* true = AI/bot player */
};

RoomPlayerInfo PlayerB[5];
RoomPlayerInfo PlayerR[5];
int  roomInfoBlueCount = 0;
int  roomInfoRedCount  = 0;
bool enableRoomInfo    = false;

/* ===================================================== */
/* Room Info — resolve SystemData & read player list     */
/* Ported from roominfo.cpp                              */
/* ===================================================== */
static uintptr_t ResolveSystemDataStaticFields() {
long a1 = getPtr641(libbase + INSTANCE_SystemData);
if (!a1) return 0;
long a2 = getPtr641(a1 + 0xB8);
if (!a2) return 0;
return (uintptr_t)((a2 << 1) >> 1);
}

void g_iRoomInfoList() {
static uint64_t lastRoomUpdate = 0;
uint64_t now = (uint64_t)time(nullptr);
if (now - lastRoomUpdate < 1) return;
lastRoomUpdate = now;

roomInfoBlueCount = 0; roomInfoRedCount = 0;
for (int i = 0; i < 5; i++) { PlayerB[i] = {}; PlayerR[i] = {}; }

uintptr_t sf = ResolveSystemDataStaticFields();
if (!sf) return;

uint64_t m_uiID = Read<uint64_t>(sf + FIELD_SD_m_uiID);
if (!m_uiID) return;

long roomDict = Read<long>(sf + FIELD_SD_m_RoomPlayerInfo);
if (!roomDict) return;

int dictCount = Read<int>(roomDict + 0x20);
if (dictCount <= 0 || dictCount > 100) return;

long entriesArray = Read<long>(roomDict + 0x18);
if (!entriesArray) return;

int entriesCap = Read<int>(entriesArray + 0x18);
if (entriesCap <= 0 || entriesCap > 500) return;

long entriesStart = entriesArray + 0x20;
long validRDs[20]; int validCount = 0;

for (int i = 0; i < entriesCap; i++) {
long entryAddr = entriesStart + (i * 0x18);
if (Read<int>(entryAddr) < 0) continue;
long listPtr = Read<long>(entryAddr + 0x10);
if (!listPtr || listPtr < 0x10000000) continue;
int listCount = Read<int>(listPtr + OFFSET_LIST_Count);
if (listCount <= 0 || listCount > 20) continue;
long listItems = Read<long>(listPtr + OFFSET_LIST_Items);
if (!listItems || listItems < 0x10000000) continue;
long itemsStart = listItems + OFFSET_ARRAY_Data;
bool found = false;
long tmp[20]; int tc = 0;
for (int j = 0; j < listCount; ++j) {
long rd = Read<long>(itemsStart + (j * 8));
if (!rd || rd < 0x10000000) continue;
if (Read<uint64_t>(rd + FIELD_RD_lUid) == m_uiID) found = true;
tmp[tc++] = rd;
}
if (found) { for (int k=0;k<tc;k++) validRDs[k]=tmp[k]; validCount=tc; }
}
if (validCount == 0) return;

int iSelfCamp = -1;
for (int i = 0; i < validCount; i++) {
if (Read<uint64_t>(validRDs[i] + FIELD_RD_lUid) == m_uiID) {
iSelfCamp = Read<int>(validRDs[i] + FIELD_RD_iCamp); break;
}
}

int Blue = 0, Red = 0;
for (int i = 0; i < validCount; i++) {
long rd = validRDs[i];
uint64_t lUid  = Read<uint64_t>(rd + FIELD_RD_lUid);
int iCamp      = Read<int>(rd + FIELD_RD_iCamp);
int uiZoneId   = Read<int>(rd + FIELD_RD_uiZoneId);
int heroid     = Read<int>(rd + FIELD_RD_heroid);
int heroChoose = Read<int>(rd + FIELD_RD_uiHeroIDChoose);
int spellId    = Read<int>(rd + FIELD_RD_summonSkillId);
uintptr_t namePtr = Read<uintptr_t>(rd + FIELD_RD_sName);
int rankLvl    = Read<int>(rd + FIELD_RD_uiRankLevel);
int mythPt     = Read<int>(rd + FIELD_RD_iMythPoint);
bool bRobot    = Read<bool>(rd + 0x48);
int dispHero   = heroChoose ? heroChoose : heroid;
std::string nm = fshy(namePtr);
std::string uid= std::to_string(lUid)+" ("+std::to_string(uiZoneId)+")";
std::string rk = RankToString(rankLvl, mythPt);
std::string hr = HeroToString(dispHero);
std::string sp = SpellToString(spellId);
if (iCamp == iSelfCamp) {
if (Blue < 5) {
PlayerB[Blue] = {nm, uid, "", rk, hr, sp, spellId, bRobot};
Blue++;
}
} else {
if (Red < 5) {
PlayerR[Red] = {nm, uid, "", rk, hr, sp, spellId, bRobot};
Red++;
}
}
}
roomInfoBlueCount = Blue;
roomInfoRedCount  = Red;
}

struct String {
char pad_0000[0x10];
int length;
wchar_t buffer[1];

const char* CString() const {
static char temp[256];
wcstombs(temp, buffer, length);
temp[length] = '\0';
return temp;
}
};

uintptr_t GetMainCamera() {
auto main_cam = Read<uintptr_t>(libbase + OFF_CAM(MainCameraPtr));
if (!main_cam)        return 0;
auto main_cam2 = Read<uintptr_t>(main_cam + OFF_CAM(CameraData));
if (!main_cam2)
return 0;
auto main_cam3 = Read<uintptr_t>(main_cam2 + OFF_CAM(CameraData2));
if (!main_cam3)
return 0;
return main_cam3;
}

struct Camera {
Matrix4x4 worldToCameraMatrix;
Matrix4x4 projectionMatrix;
};

Matrix4x4 _vMatrix;
bool WorldToScreen(Vector3 from, Vector2 *to) {
auto viewMatrix = _vMatrix.MultiplyPoint(from);    auto screenPos = Vector3(viewMatrix.X + 1.0f, viewMatrix.Y + 1.0f, viewMatrix.Z + 1.0f) / 2.0f;
*to = Vector2(screenPos.X * abs_ScreenX, abs_ScreenY - (screenPos.Y * abs_ScreenY));
return viewMatrix != Vector3::Zero();
}

void FindPoint(Vector2 origin, Vector2 &point, int screenwidth, int screenheight, int length)
{
float halfScreenWidth = screenwidth / 2.0f;
float halfScreenHeight = screenheight / 2.0f;
float halfScreenWidth2 = (screenwidth - length) / 2.0f;
float halfScreenHeight2 = (screenheight - length) / 2.0f;
float dx = fabs(origin.X - halfScreenWidth);
float dy = fabs(origin.Y - halfScreenHeight);
float rx = (dx != 0) ? halfScreenWidth2 / dx : 0;
float ry = (dy != 0) ? halfScreenHeight2 / dy : 0;
float r = fmin(rx, ry);
point.X = origin.X + (halfScreenWidth - origin.X) * (1.0f - r);
point.Y = origin.Y + (halfScreenHeight - origin.Y) * (1.0f - r);
}

int ListMonsterId[] = {
2002, 2003, 2004, 2005, 2006, 2008, 2009, 2011, 2012, 2013,
2056, 2059, 2072, 2220, 2221, 2222, 2223, 2224, 2225, 2226,
2227, 2228, 2229, 2230, 2232,};

bool bMonster(int iValue) {
return std::find(std::begin(ListMonsterId), std::end(ListMonsterId), iValue) != std::end(ListMonsterId);
}

void Touch_Tap(int x, int y) {
Touch_Down((float)x, (float)y);
usleep(30000); /* 30ms delay agar Down terkirim sebelum Up — fix ghost touch di retri */
Touch_Up();
}

bool lastRetriTriggered[20] = {false};
bool autoRetribution = false;
bool AutoRetributionRed = false;
bool AutoRetributionBlue = false;
bool AutoRetributionLord = false;
bool AutoRetributionTurtle = false;

float retriTouchX = 1575;
float retriTouchY = 661;
int g_cachedRetriDmg = 0;  /* Retri damage threshold — cached for visual bar line */
/* ===================================================== */
/* Circular Health Ring ESP                               */
/* ===================================================== */
void DrawCircularHealth(ImDrawList* Draw, float CenterX, float CenterY, float Radius, float Health, float MaxHealth, bool isMonster = false) {
float percent = (MaxHealth > 0) ? (Health / MaxHealth) : 0.0f;
if (percent < 0.0f) percent = 0.0f;
if (percent > 1.0f) percent = 1.0f;

int cr, cg, cb;
if (isMonster) {
/* Monster health: GREEN STABILO (neon green) */
cr = 50; cg = 255; cb = 50;
} else {
/* Player health: BRIGHT RED */
cr = 255; cg = 35; cb = 35;
}

float startAngle = -3.14159265f * 0.5f;
float endAngle = startAngle + (2.0f * 3.14159265f * percent);

if (percent > 0.01f) {
/* Main health arc — solid, no glow/shadow */
Draw->PathArcTo(ImVec2(CenterX, CenterY), Radius, startAngle, endAngle, 44);
Draw->PathStroke(IM_COL32(cr, cg, cb, 255), false, 4.5f);
}
}


namespace Prediction {

namespace Fields {
constexpr size_t m_vCachePosition    = 0x294;        constexpr size_t _MoveDir            = 0x2d4;
constexpr size_t _vecFaceDir         = 0x2e0;
constexpr size_t curSpeed            = 0x488;
constexpr size_t m_dMoveSpeed        = 0x230;
constexpr size_t m_dRunSpeed         = 0x268;
constexpr size_t m_bIsInMoveControl  = 0x37a;
}

struct EntityPrediction {
Vector3 position;
Vector3 moveDir;
float speed;
bool isValid;
bool isMoving;
float lastUpdateTime;
};    
struct PredictionCache {
uintptr_t entityAddr;
EntityPrediction pred;
bool active;
};

PredictionCache predCache[150];
constexpr int MAX_CACHE = 150;

constexpr float VISUAL_SCALE = 1.0f;
constexpr float MIN_SPEED_THRESHOLD = 0.3f;
constexpr float MAX_VISUAL_LENGTH = 120.0f;

inline float GetEntitySpeed(uintptr_t entityAddr) {
float speed = Read<float>(entityAddr + Fields::curSpeed);
if (speed > 0.01f) return speed;

double moveSpeed = Read<double>(entityAddr + Fields::m_dMoveSpeed);
if (moveSpeed > 0.01) return static_cast<float>(moveSpeed);

double runSpeed = Read<double>(entityAddr + Fields::m_dRunSpeed);
return static_cast<float>(runSpeed);
}

inline Vector3 GetEntityMoveDir(uintptr_t entityAddr) {
Vector3 moveDir = Read<Vector3>(entityAddr + Fields::_MoveDir);
float len = sqrtf(moveDir.X*moveDir.X + moveDir.Y*moveDir.Y + moveDir.Z*moveDir.Z);

if (len > 0.1f) {
return { moveDir.X/len, moveDir.Y/len, moveDir.Z/len };
}

Vector3 faceDir = Read<Vector3>(entityAddr + Fields::_vecFaceDir);
len = sqrtf(faceDir.X*faceDir.X + faceDir.Y*faceDir.Y + faceDir.Z*faceDir.Z);        if (len > 0.1f) {
return { faceDir.X/len, faceDir.Y/len, faceDir.Z/len };
}

return {0, 0, 0};
}

void UpdatePrediction(uintptr_t entityAddr, Vector3 currentPos, float currentTime) {
for (int i = 0; i < MAX_CACHE; i++) {
if (predCache[i].active && predCache[i].entityAddr == entityAddr) {
float speed = GetEntitySpeed(entityAddr);
Vector3 moveDir = GetEntityMoveDir(entityAddr);

predCache[i].pred.position = currentPos;
predCache[i].pred.moveDir = moveDir;                predCache[i].pred.speed = speed;
predCache[i].pred.isValid = true;
predCache[i].pred.isMoving = (speed > MIN_SPEED_THRESHOLD && (moveDir.X != 0 || moveDir.Z != 0));
predCache[i].pred.lastUpdateTime = currentTime;
return;
}
}

for (int i = 0; i < MAX_CACHE; i++) {
if (!predCache[i].active) {
predCache[i].entityAddr = entityAddr;
predCache[i].pred.position = currentPos;
predCache[i].pred.moveDir = GetEntityMoveDir(entityAddr);
predCache[i].pred.speed = GetEntitySpeed(entityAddr);
predCache[i].pred.isValid = true;
predCache[i].pred.isMoving = (predCache[i].pred.speed > MIN_SPEED_THRESHOLD);
predCache[i].pred.lastUpdateTime = currentTime;
predCache[i].active = true;
return;
}
}
}

EntityPrediction* GetPrediction(uintptr_t entityAddr) {
for (int i = 0; i < MAX_CACHE; i++) {
if (predCache[i].active && predCache[i].entityAddr == entityAddr) {
return &predCache[i].pred;
}
}
return nullptr;
}

void InvalidatePrediction(uintptr_t entityAddr) {
for (int i = 0; i < MAX_CACHE; i++) {
if (predCache[i].active && predCache[i].entityAddr == entityAddr) {
predCache[i].active = false;                return;
}
}
}

void CleanupStale(float currentTime, float maxAge = 2.0f) {
for (int i = 0; i < MAX_CACHE; i++) {
if (predCache[i].active) {
float age = currentTime - predCache[i].pred.lastUpdateTime;
if (age > maxAge) {
predCache[i].active = false;
}
}
}    }

inline float CalcVisualLength(float speed) {
float raw = speed * 8.0f * VISUAL_SCALE;
return fminf(raw, MAX_VISUAL_LENGTH);
}

inline ImU32 GetSpeedColor(float speed) {
float clamped = fminf(fmaxf(speed, 1.0f), 20.0f);
float t = (clamped - 1.0f) / 19.0f;

if (t < 0.33f) {
float lt = t / 0.33f;
return IM_COL32(static_cast<int>(255.0f * lt), 255, 0, 200);
} else if (t < 0.66f) {
float lt = (t - 0.33f) / 0.33f;
return IM_COL32(255, static_cast<int>(255.0f * (1.0f - lt * 0.5f)), 0, 200);
} else {
float lt = (t - 0.66f) / 0.34f;
return IM_COL32(255, static_cast<int>(127.0f * (1.0f - lt)), 0, 200);
}
}
}

bool drawMPrediction = false;
bool predictionColorBySpeed = true;
ImColor predictionLineColor = ImColor(0, 220, 255, 180);
ImColor predictionCircleColor = ImColor(240, 240, 255, 190);
float predictionCircleRadius = 5.0f;
bool predictionShowOnlyMoving = true;
bool predictionShowArrowHead = true;
bool predictionShowSpeedText = false;

void DrawPrediction(ImDrawList* draw, Vector2 screenPos, Vector3 worldPos, uintptr_t entityAddr) {
using namespace Prediction;

float currentTime = static_cast<float>(std::clock()) / CLOCKS_PER_SEC;    
UpdatePrediction(entityAddr, worldPos, currentTime);

EntityPrediction* pred = GetPrediction(entityAddr);
if (!pred || !pred->isValid) return;

if (predictionShowOnlyMoving && !pred->isMoving) return;

if (pred->moveDir.X == 0 && pred->moveDir.Y == 0 && pred->moveDir.Z == 0) return;    
constexpr float TIME_VISUAL_FACTOR = 1.0f;

Vector3 predictedWorld = {
worldPos.X + pred->moveDir.X * pred->speed * TIME_VISUAL_FACTOR,        worldPos.Y + pred->moveDir.Y * pred->speed * TIME_VISUAL_FACTOR,
worldPos.Z + pred->moveDir.Z * pred->speed * TIME_VISUAL_FACTOR
};

Vector2 predictedScreen;
if (!WorldToScreen(predictedWorld, &predictedScreen)) return;

float lineLength = CalcVisualLength(pred->speed);
ImU32 lineColor = predictionColorBySpeed ? GetSpeedColor(pred->speed) : IM_COL32(0, 220, 255, 200);
float lineThickness = 1.5f + fminf(pred->speed * 0.1f, 2.0f);

draw->AddLine(
ImVec2(screenPos.X, screenPos.Y),
ImVec2(predictedScreen.X, predictedScreen.Y),
lineColor,
lineThickness
);

if (predictionShowArrowHead && lineLength > 20.0f) {
float arrowSize = fminf(10.0f, lineLength * 0.3f);
float angle = atan2f(predictedScreen.Y - screenPos.Y, predictedScreen.X - screenPos.X);

ImVec2 arrow1 = {
predictedScreen.X - arrowSize * cosf(angle - 0.4f),
predictedScreen.Y - arrowSize * sinf(angle - 0.4f)
};
ImVec2 arrow2 = {
predictedScreen.X - arrowSize * cosf(angle + 0.4f),
predictedScreen.Y - arrowSize * sinf(angle + 0.4f)
};

draw->AddLine(ImVec2(predictedScreen.X, predictedScreen.Y), arrow1, lineColor, lineThickness * 0.9f);
draw->AddLine(ImVec2(predictedScreen.X, predictedScreen.Y), arrow2, lineColor, lineThickness * 0.9f);
}

draw->AddCircle(
ImVec2(predictedScreen.X, predictedScreen.Y),
predictionCircleRadius,        predictionCircleColor,
14,
1.2f
);

draw->AddCircleFilled(
ImVec2(predictedScreen.X, predictedScreen.Y),
predictionCircleRadius * 0.35f,
predictionCircleColor
);

if (predictionShowSpeedText && pred->speed > 1.0f) {        char speedTxt[12];
snprintf(speedTxt, sizeof(speedTxt), "%.1f", pred->speed);
ImVec2 txtSize = ImGui::CalcTextSize(speedTxt);
draw->AddText(
ImVec2(predictedScreen.X + 6.0f, predictedScreen.Y - txtSize.y * 0.5f),
IM_COL32(255, 255, 255, 220),
speedTxt
);
}
}
uintptr_t GetLogicBattleManagerInstance() {
auto logicBattleManager = Read<uintptr_t>(libbase + OFF_BLC(BasePtr));
if (!logicBattleManager)
return 0;

auto staticPtr = Read<uintptr_t>(logicBattleManager + OFF_BLC(PtrChain1));
if (!staticPtr)
return 0;

auto instancePtr = Read<uintptr_t>(staticPtr);
if (!instancePtr)
return 0;

return instancePtr;
}

/* ===================================================== */
/* CoolDownData Struct & Helper */
/* ===================================================== */
struct CoolDownData {
int skill1;  // Skill slot 1 (fSkillID 10)
int skill2;  // Skill slot 2 (fSkillID 20)
int skill3;  // Skill slot 3 (fSkillID 30)
int skill4;  // Skill slot 4 (fSkillID 40)
int spell;   // Retribution / Spell (fSkillID 10, skillID >= 20000)

CoolDownData() : skill1(0), skill2(0), skill3(0), skill4(0), spell(0) {}
};

bool ESP_Player_Cooldown = true;
bool Cooldown_ShowSpell = true;
bool Cooldown_CompactStyle = false;
ImColor Cooldown_ColorReady = ImColor(0, 255, 130, 255);
ImColor Cooldown_ColorCD = ImColor(255, 50, 80, 255);
float Cooldown_OffsetY = 35.0f;

CoolDownData getCoolDown(uintptr_t show_entity) {
CoolDownData result;

auto cdComp = Read<uintptr_t>(show_entity + OFF_SE(m_ShowCoolDownComp));
if (!cdComp) return result;

auto dicCoolInfoPtr = Read<uintptr_t>(cdComp + OFF_SCC(_dicCD));
if (!dicCoolInfoPtr) return result;

monoDictionary<int, uintptr_t> dict{};
dict.klass        = reinterpret_cast<void*>(Read<uintptr_t>(dicCoolInfoPtr + 0x0));
dict.obj          = reinterpret_cast<void*>(Read<uintptr_t>(dicCoolInfoPtr + 0x8));
dict.buckets      = reinterpret_cast<int32_t*>(Read<uintptr_t>(dicCoolInfoPtr + 0x10));
dict.arrayEntries = Read<uintptr_t>(dicCoolInfoPtr + 0x18);
dict.count        = Read<int>(dicCoolInfoPtr + 0x20);

if (!dict.arrayEntries || dict.count <= 0) return result;

/* 4. Get current frame time for cooldown calculation */
auto logicMgr = GetLogicBattleManagerInstance();
if (!logicMgr) return result;

auto now = Read<uint>(logicMgr + OFF_BLC(m_uiFrameTime));

/* 5. Iterate dictionary entries */
auto items = get_mono_dictionary_vector<int, uintptr_t>(dict);
for (const auto& entry : items) {
auto skillID = entry.key;
auto coolDownDataAddr = entry.value;

if (skillID < 0 || !coolDownDataAddr) continue;

/* Read cooldown fields */
auto startTime  = Read<uint>(coolDownDataAddr + OFF_CDD(uiStartTime));
auto coolTime   = Read<uint>(coolDownDataAddr + OFF_CDD(uiCoolTime));
auto endTime    = startTime + coolTime;
if (endTime < now) endTime = now;

auto timeLeftSec = static_cast<int>((endTime - now) / 1000u);
if (timeLeftSec < 0) timeLeftSec = 0;

auto fSkillID = skillID % 100;

/* Map to struct fields */
if (skillID < 20000) {
/* Basic skills: 10,20,30,40 -> skill1~4 */
switch (fSkillID) {
case 10: result.skill1 = timeLeftSec; break;
case 20: result.skill2 = timeLeftSec; break;
case 30: result.skill3 = timeLeftSec; break;
case 40: result.skill4 = timeLeftSec; break;
}
} else {
/* Spell / Retribution: skillID >= 20000 && fSkillID == 10 */
if (fSkillID == 10 || (skillID <= 20500 && (skillID % 10) == 0)) {
result.spell = timeLeftSec;
}
}
}

return result;
}

struct MonsterData {
uintptr_t address;
Vector3 position;
float distance;
int health;
int maxHP;
int heroID;
bool isDead;
bool isVisible;
bool isValid;
char name[100];
};

MonsterData monster[20];
int MonsterCount = 0;
uintptr_t Oneself;

/* ===================================================== */
/* Lord/Turtle Under Attack — Global Health Bar Alert    */
/* Uses monster[] array from MonsterRetribution()        */
/* Works regardless of player position on the map        */
/* ===================================================== */
void DrawLordTurtleAlertBar(ImDrawList* Draw) {
if (!drawAlertUnderAttack) return;
static float lastLordBelowMaxTime  = -10.0f;
static float lastTurtleBelowMaxTime = -10.0f;
static int prevLordHP = -1;
static int prevTurtleHP = -1;
static auto _alertBase = std::chrono::steady_clock::now();
float currentTime = std::chrono::duration<float>(
std::chrono::steady_clock::now() - _alertBase).count();

int snapCount = MonsterCount;
if (snapCount > 20) snapCount = 20;

/* Track whether Lord/Turtle was found ALIVE this frame */
bool lordFoundAlive = false;
bool turtleFoundAlive = false;
for (int s = 0; s < snapCount; s++) {
if (!monster[s].isValid || monster[s].address == 0) continue;
int sid = monster[s].heroID;

if (sid == 2002) {
if (monster[s].isDead || monster[s].health <= 0) {
/* Lord DEAD → reset tracking + timer for clean respawn */
prevLordHP = -1;
lastLordBelowMaxTime = -10.0f; /* Reset so bar disappears on respawn */
} else {
lordFoundAlive = true;
int shp = monster[s].health;
int smax = monster[s].maxHP;
if (smax > 0 && shp > 0) {
if (shp < smax || (prevLordHP > 0 && shp < prevLordHP)) {
lastLordBelowMaxTime = currentTime;
}
prevLordHP = shp;
}
}
}
if (sid == 2003) {
if (monster[s].isDead || monster[s].health <= 0) {
prevTurtleHP = -1;
lastTurtleBelowMaxTime = -10.0f; /* Reset so bar disappears on respawn */
} else {
turtleFoundAlive = true;
int shp = monster[s].health;
int smax = monster[s].maxHP;
if (smax > 0 && shp > 0) {
if (shp < smax || (prevTurtleHP > 0 && shp < prevTurtleHP)) {
lastTurtleBelowMaxTime = currentTime;
}
prevTurtleHP = shp;
}
}
}
}

/* Entity not found this frame — may be a 1-frame race gap in MonsterRetribution.
Do NOT reset prevHP here; death-based reset already handled above (lines
609-611 / 624-626). Resetting here corrupts prevHP on momentary gaps and
breaks the 'shp < prevHP' damage-detection path on the very next frame. */
if (!lordFoundAlive)   { /* keep prevLordHP  — survive 1-frame gaps */ }
if (!turtleFoundAlive) { /* keep prevTurtleHP — survive 1-frame gaps */ }

if (snapCount <= 0) return;

float barY = 65.0f;

for (int i = 0; i < snapCount; i++) {
if (!monster[i].isValid || monster[i].isDead) continue;
if (monster[i].address == 0) continue;

/* FIX: Use cached heroID instead of live Read<int>() */
int id = monster[i].heroID;

bool isLord   = (id == 2002);
bool isTurtle = (id == 2003);
if (!isLord && !isTurtle) continue;

/* FIX: Use cached HP values instead of live memory reads */
int liveHP    = monster[i].health;
int liveMaxHP = monster[i].maxHP;
if (liveMaxHP <= 0 || liveHP <= 0) continue;

/* Check if should show: HP below max OR within 4s timeout */
bool showBar = false;
if (liveHP < liveMaxHP) {
showBar = true;
/* Dual-path refresh: keep timer alive during ongoing attack even
if the pre-scan above missed a frame due to race condition */
if (isLord)   lastLordBelowMaxTime   = currentTime;
if (isTurtle) lastTurtleBelowMaxTime = currentTime;
} else {
/* HP is full — check timeout */
if (isLord   && (currentTime - lastLordBelowMaxTime)  <= 4.0f) showBar = true;
if (isTurtle && (currentTime - lastTurtleBelowMaxTime) <= 4.0f) showBar = true;
}
if (!showBar) continue;

float percent = (float)liveHP / (float)liveMaxHP;
if (percent > 1.0f) percent = 1.0f;
if (percent < 0.0f) percent = 0.0f;

/* Bar dimensions */
float barWidth  = abs_ScreenX * 0.55f;
float barHeight = 26.0f;
float barX      = (abs_ScreenX - barWidth) / 2.0f;

/* Label */
const char* label = isLord ? "LORD" : "TURTLE";
char hpText[64];
snprintf(hpText, sizeof(hpText), "%s  %.0f%%", label, percent * 100.0f);

/* Color: Lord = Orange-Red, Turtle = Cyan-Teal */
int cr, cg, cb;
if (isLord) {
cr = 255; cg = 100; cb = 30;
} else {
cr = 0; cg = 220; cb = 200;
}

/* Glow effect — outer layer (wide, soft, RED) */
Draw->AddRectFilled(
ImVec2(barX - 5.0f, barY - 5.0f),
ImVec2(barX + barWidth + 5.0f, barY + barHeight + 5.0f),
IM_COL32(220, 30, 30, 30), 10.0f);

/* Glow effect — middle layer (RED) */
Draw->AddRectFilled(
ImVec2(barX - 3.0f, barY - 3.0f),
ImVec2(barX + barWidth + 3.0f, barY + barHeight + 3.0f),
IM_COL32(220, 30, 30, 55), 8.0f);

/* Background bar — dark for RED fill contrast */
Draw->AddRectFilled(
ImVec2(barX, barY),
ImVec2(barX + barWidth, barY + barHeight),
IM_COL32(40, 10, 10, 230), 8.0f);

/* Health fill bar — RED fill */
float fillWidth = barWidth * percent;
if (fillWidth > 0.0f) {
Draw->AddRectFilled(
ImVec2(barX, barY),
ImVec2(barX + fillWidth, barY + barHeight),
IM_COL32(220, 25, 25, 248), 6.0f);
}

/* Retri threshold line — VISUAL ONLY, does not affect auto-retri logic */
if (g_cachedRetriDmg > 0 && liveMaxHP > 0) {
float retriPct = (float)g_cachedRetriDmg / (float)liveMaxHP;
if (retriPct > 0.0f && retriPct <= 1.0f) {
float retriLineX = barX + barWidth * retriPct;
/* Outer glow line (wider, semi-transparent) */
Draw->AddLine(
ImVec2(retriLineX, barY),
ImVec2(retriLineX, barY + barHeight),
IM_COL32(255, 220, 50, 80), 4.0f);
/* Inner crisp line */
Draw->AddLine(
ImVec2(retriLineX, barY + 1.0f),
ImVec2(retriLineX, barY + barHeight - 1.0f),
IM_COL32(255, 230, 60, 240), 1.5f);
}
}

/* Border — RED accent for both LORD and TURTLE */
Draw->AddRect(
ImVec2(barX, barY),
ImVec2(barX + barWidth, barY + barHeight),
IM_COL32(255, 40, 40, 220), 8.0f, 0, 2.2f);

/* Text label centered on bar */
auto textSize = ImGui::CalcTextSize(hpText);
float textX = barX + (barWidth - textSize.x) / 2.0f;
float textY = barY + (barHeight - textSize.y) / 2.0f;

/* Text shadow — same tint as bar color (no black) */
Draw->AddText(ImVec2(textX + 1.0f, textY + 1.0f), IM_COL32(cr/2, cg/2, cb/2, 200), hpText);
/* Main text */
Draw->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 255), hpText);

barY += barHeight + 14.0f;  /* Stack multiple bars if both Lord & Turtle under attack */
}
}

/* ===================================================== */
/* Cooldown Display Helper */
/* ===================================================== */
void DrawMonster(ImDrawList *Draw) {
using namespace Prediction;
float currentTime = static_cast<float>(std::clock()) / CLOCKS_PER_SEC;
CleanupStale(currentTime);
DrawLordTurtleAlertBar(Draw);

if (autoRetribution) {
ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(retriTouchX, retriTouchY), 18.0f, IM_COL32(0, 220, 255, 180), 16);
ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(retriTouchX, retriTouchY), 18.0f, IM_COL32(0, 0, 0, 255), 16, 2.5f);
}
if (abs_ScreenX < abs_ScreenY) return;    
float lineSize = abs_ScreenY / 432;
long a1 = ReadPtr(libbase + OFF_BM(BasePtr));
long a2 = ReadPtr((a1 + ((0x100 | OFF_BM(PtrChain1)) & 0xFF)));
long a32 = ReadPtr((a2 << 1) >> 1);

long selfp = ReadPtr(a32 + OFF_BM(LocalPlayerShow));

auto main_cam = GetMainCamera();
auto camera = Read<uintptr_t>(main_cam + OFF_CAM(SmoothFollow));
if (EnableDrone) Write<float>(camera + OFFSET_CAM_FOV, FieldView);
if (EnableDroneV2 && DroneHeight != 0.0f) Write<float>(camera + 0xD8, DroneHeight);
auto ViewMatrix = Read<Camera>(camera + OFF_CAM(ViewMatrix));
_vMatrix = ViewMatrix.projectionMatrix * ViewMatrix.worldToCameraMatrix;

long player = ReadPtr(ReadPtr(a32 + OFF_BM(ShowPlayers)) + OFF_BM(ListDataOffset)) + OFF_BM(ListArrayOffset);
uint stop_player = Read<uint>(ReadPtr(a32 + OFF_BM(ShowPlayers)) + OFF_BM(ListCountOffset));

for (int i = 0; i < stop_player; i++) {
auto Objaddr = ReadPtr(player + ((i << 3) / 1));
if ((Objaddr ^ 0x0) == 0x0) continue;

auto is_team = Read<bool>(Objaddr + OFF_SE(bSameCampType));
if (is_team) continue;

auto HeroID = Read<int>(Objaddr + OFF_SE(HeroID));
auto death = Read<bool>(Objaddr + OFF_SE(bDeath));

if (death) {
Prediction::InvalidatePrediction(Objaddr);
continue;        }

int Health = Read<uint64_t>(Objaddr + OFF_SE(Hp));
if(Health <= 0) continue;

int maxHealth = Read<uint64_t>(Objaddr + OFF_SE(HpMax));
if(maxHealth <= 0) continue;

Vector3 Z;
vm_readv(selfp + OFF_SE(vCachePosition), &Z, sizeof(Z));

Vector3 D;
vm_readv(Objaddr + OFF_SE(vCachePosition), &D, sizeof(D));

Vector2 en_posSc;
WorldToScreen(D, &en_posSc);

Vector2 loc_posSc;
WorldToScreen(Z, &loc_posSc);

bool isOutScreen;
float IconSize = abs_ScreenX / 10.4;
Vector2 HeroPos = {en_posSc.X, en_posSc.Y};
Vector2 Res;

if (HeroPos.X < 0 || HeroPos.X > abs_ScreenX || HeroPos.Y < 0 || HeroPos.Y > abs_ScreenY) {
isOutScreen = true;
IconSize = abs_ScreenX / 15.6;
FindPoint(HeroPos, Res, abs_ScreenX, abs_ScreenY, (IconSize / 3));
} else {
isOutScreen = false;
Res = HeroPos;
}

auto Distance = Vector3::Distance(Z, D);

if (drawMHealth) {
ImGui::GetForegroundDrawList()->AddCircleFilled({en_posSc.X, en_posSc.Y}, lineSize * 1.8f, IM_COL32(255, 50, 150, 220), 12);
DrawCircularHealth(Draw, HeroPos.X, HeroPos.Y, 28.0f, (float)Health, (float)maxHealth);
/* Hero icon inside health circle at 50% opacity */
if (iconhero) {
ImTextureID htex = GetHeroTexture(HeroID);
if (htex) {
float iconR = 22.0f;
Draw->AddImageRounded(htex,
ImVec2(HeroPos.X - iconR, HeroPos.Y - iconR),
ImVec2(HeroPos.X + iconR, HeroPos.Y + iconR),
ImVec2(0,0), ImVec2(1,1),
IM_COL32(255, 255, 255, 230), iconR); /* 90% opacity */
}
}
}

if (drawMPrediction) {
DrawPrediction(Draw, HeroPos, D, Objaddr);
}

if (drawMDistance) {
std::string nickname = fshy(Read<uintptr_t>(Objaddr + OFF_SP(HeroName)));
std::string distanceStr = std::to_string((int)Distance) + "m | ";            std::string infoText = distanceStr /*+ nickname*/;

auto textSize1 = ImGui::CalcTextSize(infoText.c_str(), 0, 20);
绘制字体描边(22.5, HeroPos.X - (textSize1.x / 2), HeroPos.Y - 20.0f, ImColor(0, 220, 255), infoText.c_str());

if (HeroID <= 139) {
std::string heroName = HeroToString(HeroID);
if (!heroName.empty()) {
auto textSize2 = ImGui::CalcTextSize(heroName.c_str(), 0, 29);
绘制字体描边(22.5, HeroPos.X - (textSize2.x / 2), HeroPos.Y + 15.0f, ImColor(255, 200, 50), heroName.c_str());
}
}
}
}

long monster = ReadPtr(ReadPtr(a32 + OFF_BM(ShowMonsters)) + OFF_BM(ListDataOffset)) + OFF_BM(ListArrayOffset);
uint stop_monster = Read<uint>(ReadPtr(a32 + OFF_BM(ShowMonsters)) + OFF_BM(ListCountOffset));

for (int i = 0; i < stop_monster; i++) {
auto Objaddr = ReadPtr(monster + ((i << 3) / 1));

if ((Objaddr ^ 0x0) == 0x0) continue;

auto is_team = Read<bool>(Objaddr + OFF_SE(bSameCampType));
if (is_team) continue;

auto mHeroID = Read<int>(Objaddr + OFF_SE(HeroID));        
auto type = Read<int>(Objaddr + OFF_SE(iType));        
auto death = Read<bool>(Objaddr + OFF_SE(bDeath));

if (death) {
Prediction::InvalidatePrediction(Objaddr);
continue;
}

int Health = Read<uint64_t>(Objaddr + OFF_SE(Hp));
if(Health <= 0) continue;

int maxHealth = Read<uint64_t>(Objaddr + OFF_SE(HpMax));
if(maxHealth <= 0) continue;     
Vector3 ZL;
vm_readv(selfp + OFF_SE(vCachePosition), &ZL, sizeof(ZL));

Vector3 Dm;
vm_readv(Objaddr + OFF_SE(vCachePosition), &Dm, sizeof(Dm));

Vector2 mon_posSc;
WorldToScreen(Dm, &mon_posSc);
Vector2 MonPos = {mon_posSc.X, mon_posSc.Y};
if (MonPos.X < 0 || MonPos.X > abs_ScreenX || MonPos.Y < 0 || MonPos.Y > abs_ScreenY) continue;     
if (type == 2 || type == 5) {
if (!bMonster(mHeroID)) continue;               

/* Health ring: ONLY for Buff monsters (Red Buff, Blue Buff) */
if (drawMHealth && (mHeroID == 2004 || mHeroID == 2005 || mHeroID == 2220 || mHeroID == 2221)) {
DrawCircularHealth(Draw, MonPos.X, MonPos.Y, 24.0f, (float)Health, (float)maxHealth, true);
}            

/* Show HP number for Buff monsters (Red Buff, Blue Buff) */
if (mHeroID == 2004 || mHeroID == 2005 || mHeroID == 2220 || mHeroID == 2221) {
char hpBuf[32];
snprintf(hpBuf, sizeof(hpBuf), "%d", Health);
auto hpTextSize = ImGui::CalcTextSize(hpBuf);
float hpX = MonPos.X - (hpTextSize.x / 2.0f);
float hpY = MonPos.Y + 28.0f;
/* Shadow — same color, slightly offset (no black) */
ImU32 hpColor = (mHeroID == 2004 || mHeroID == 2220)
? IM_COL32(255, 140, 50, 255)
: IM_COL32(50, 180, 255, 255);
ImU32 hpShadow = (mHeroID == 2004 || mHeroID == 2220)
? IM_COL32(180, 90, 20, 200)
: IM_COL32(20, 120, 180, 200);
Draw->AddText(ImVec2(hpX + 1.0f, hpY + 1.0f), hpShadow, hpBuf);
Draw->AddText(ImVec2(hpX, hpY), hpColor, hpBuf);
}

if (drawMPrediction) {
DrawPrediction(Draw, MonPos, Dm, Objaddr);
}
}
}
}

/* MonsterData struct + globals moved above DrawLordTurtleAlertBar */

void MonsterRetribution() {
uintptr_t BattleManager = ReadPtr(libbase + OFF_BM(BasePtr));
BattleManager = ReadPtr(BattleManager + OFF_BM(PtrChain1));
BattleManager = ReadPtr(BattleManager);

if(!BattleManager) return;
Oneself = ReadPtr(BattleManager + OFF_BM(LocalPlayerShow));
if(!Oneself) return;

Vector3 MyPosition;
vm_readv(Oneself + OFF_SE(vCachePosition), &MyPosition, sizeof(MyPosition));

/* FIX: Use temp array + temp counter to avoid setting MonsterCount=0
mid-frame which causes the render thread to see empty data → flicker */
MonsterData tempMonster[20];
int tempCount = 0;

uintptr_t Showmonster = ReadPtr(BattleManager + OFF_BM(ShowMonsters));
if (Showmonster != 0) {
int monsterCount = Read<int>(Showmonster + OFF_BM(ListCountOffset));
uintptr_t monsterDataPtr = ReadPtr(Showmonster + OFF_BM(ListDataOffset));
if (monsterCount >= 0 && monsterCount <= 100 && monsterDataPtr != 0) {
uintptr_t monsterDataArray = monsterDataPtr + OFF_BM(ListArrayOffset);
for (int i = 0; i < monsterCount && tempCount < 20; i++) {
uintptr_t currentMonsterPtr = ReadPtr(monsterDataArray + (i * 8));
if (currentMonsterPtr == 0) continue;

int monsterID = Read<int>(currentMonsterPtr + OFF_SE(HeroID));
int monsterHP = Read<int>(currentMonsterPtr + OFF_SE(Hp));
int monsterMaxHP = Read<int>(currentMonsterPtr + OFF_SE(HpMax));
Vector3 monsterPos = Read<Vector3>(currentMonsterPtr + OFF_SE(vCachePosition));
uint8_t deadFlag = Read<uint8_t>(currentMonsterPtr + OFF_SE(bDeath));
bool mDead = (deadFlag != 0);

int mType = Read<int>(currentMonsterPtr + OFF_SE(iType));
if (mType != 2 && mType != 5) continue;                

std::string mName = MonsterToString(monsterID);
if (mName.empty()) {
if (monsterID == 2002) mName = "Lord";
else if (monsterID == 2003) mName = "Turtle";
else if (monsterID == 2004) mName = "Red Buff";
else if (monsterID == 2005) mName = "Blue Buff";
else continue;
}

tempMonster[tempCount].address   = currentMonsterPtr;
tempMonster[tempCount].position  = monsterPos;
tempMonster[tempCount].distance  = Vector3::Distance(MyPosition, monsterPos);
tempMonster[tempCount].health    = monsterHP;
tempMonster[tempCount].maxHP     = monsterMaxHP;
tempMonster[tempCount].heroID    = monsterID;
tempMonster[tempCount].isDead    = mDead;
tempMonster[tempCount].isVisible = true;
tempMonster[tempCount].isValid   = true;
strncpy(tempMonster[tempCount].name, mName.c_str(), sizeof(tempMonster[tempCount].name) - 1);
tempMonster[tempCount].name[sizeof(tempMonster[tempCount].name) - 1] = '\0';
tempCount++;
}
}
}

/* Atomic-style swap: copy temp data then update count last */
memcpy(monster, tempMonster, sizeof(MonsterData) * tempCount);
/* Clear remaining slots */
for (int i = tempCount; i < 20; i++) monster[i].isValid = false;
MonsterCount = tempCount;
}

int CalculateRetriDamage(int m_Level, int _KillWildTimes, int _killNum, int _assistNum)
{
    if (_KillWildTimes < 5)
    {
        return (int)(600.f + (float)(m_Level - 1) * 80.f);
    }
    else
    {
        return (int)(
            (600.f + (float)(m_Level - 1) * 80.f) +
            (float)(300 + (m_Level - 1) * 40)
        );
    }
}

void CheckAndTriggerRetribution() {    if (!autoRetribution || !Oneself || MonsterCount <= 0) return;
int myLevel = Read<int>(Oneself + OFF_SE(Level));
int killWild = Read<int>(Oneself + OFF_SE(KillWild));
int killNum = Read<int>(Oneself + OFF_SE(m_killNum));
int assistNum = Read<int>(Oneself + OFF_SE(m_assistNum));
int retriDmg = CalculateRetriDamage(myLevel, killWild, killNum, assistNum);
g_cachedRetriDmg = retriDmg; /* Cache for retri line visual in alert bar */
for (int i = 0; i < MonsterCount; i++) {
if (!monster[i].isValid || monster[i].isDead) continue;
if (monster[i].distance > 4.0f) continue;

int id = Read<int>(monster[i].address + OFF_SE(HeroID));
bool isTarget = false;

if (AutoRetributionLord && id == 2002) isTarget = true;
else if (AutoRetributionTurtle && (id == 2003 || id == 2110)) isTarget = true;
else if (AutoRetributionBlue && (id == 2005 || id == 2221)) isTarget = true;
else if (AutoRetributionRed && (id == 2004 || id == 2220)) isTarget = true;

if (!isTarget) continue;

int realHP = Read<int>(monster[i].address + OFF_SE(Hp));
uint8_t realDead = Read<uint8_t>(monster[i].address + OFF_SE(bDeath));

if (realDead != 0 || realHP <= 0) continue;

if (realHP <= retriDmg) {
Touch_Tap(retriTouchX, retriTouchY);
}
}
}

int MinimapSize = 341;
int MinimapPos = 99;
bool MinimapIcon = true;
bool HideLine = false;

float g_MinimapScale = 74.11f;
float g_Res0_MultX = 1.0f;
float g_Res0_MultY = 1.0f;
float g_Res1_OffsetX = 0.0f;
float g_Res1_OffsetY = 0.0f;
int g_ICSize = 26;

Vector2 WorldToMinimap(Vector3 HeroPosition) {
int CampType = Read<int>(Oneself + OFF_SP(m_EntityCampType));
float angle = (CampType == 2 ? 314.60f : 134.76f) * 0.017453292519943295;
float angleCos = std::cos(angle);
float angleSin = std::sin(angle);
Vector2 Res0;    float worldX = -HeroPosition.X;
float worldZ = HeroPosition.Y; 

Res0.X = ((angleCos * worldX - angleSin * worldZ) / g_MinimapScale) * g_Res0_MultX;
Res0.Y = ((angleSin * worldX + angleCos * worldZ) / g_MinimapScale) * g_Res0_MultY;
Vector2 Res1;
Res1.X = (Res0.X * MinimapSize) + MinimapPos + (MinimapSize / 2.0f) + g_Res1_OffsetX;
Res1.Y = (Res0.Y * MinimapSize) + (MinimapSize / 2.0f) + g_Res1_OffsetY;

return Res1;
}

void DrawMinimapESP(ImDrawList* draw) {
if (!MinimapIcon) return;

long a1 = ReadPtr(libbase + OFF_BM(BasePtr));
if (!a1) return;
long a2 = ReadPtr(a1 + OFF_BM(PtrChain1));
if (!a2) return;
long a32 = ReadPtr(a2);
if (!a32) return;

long showList = ReadPtr(a32 + OFF_BM(ShowPlayers));
if (!showList) return;
long playerList = ReadPtr(showList + OFF_BM(ListDataOffset));
if (!playerList) return;
playerList += OFF_BM(ListArrayOffset);

uint playerCount = Read<uint>(showList + OFF_BM(ListCountOffset));
for (int i = 0; i < playerCount; i++) {
long Objaddr = ReadPtr(playerList + (i << 3));
if (!Objaddr) continue;

if (Read<bool>(Objaddr + OFF_SE(bSameCampType))) continue;
if (Read<bool>(Objaddr + OFF_SE(bDeath))) continue;

Vector3A pos;
vm_readv(Objaddr + OFF_SE(vCachePosition), &pos, sizeof(pos));
if (pos.X == 0 && pos.Y == 0 && pos.Z == 0) continue;

Vector2 minimapPos = WorldToMinimap({ pos.X, pos.Y, pos.Z });
int heroId = Read<int>(Objaddr + OFF_SE(HeroID));
int hp = Read<int>(Objaddr + OFF_SE(Hp));
int maxHp = Read<int>(Objaddr + OFF_SE(HpMax));

DrawHeroIcon(draw, ImVec2(minimapPos.X, minimapPos.Y), heroId, hp, maxHp, g_ICSize / 2.0f);
}


if (!HideLine) {
draw->AddRect(
ImVec2(MinimapPos, 0),
ImVec2(MinimapPos + MinimapSize, MinimapSize),
IM_COL32(220, 50, 50, 255)
);
}
}

#include <iostream>
#include <vector>
#include <cmath>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

namespace AutoAim {
inline bool enabled = false;
inline float fovRadius = 150.0f;
inline float smoothFactor = 0.15f;    inline int priorityMode = 0;
inline bool showVisuals = false;

inline bool testMode = false;
inline float testAngle = 0.0f;
inline float testSpeed = 1.5f;

inline float joyCenterX = 0.0f;
inline float joyCenterY = 0.0f;
inline float joyRadius = 180.0f;
inline float joyDeadZone = 15.0f;
inline bool joyVisible = false;
inline bool joyDragging = false;
inline bool joyActive = false;
inline float joyAngle = 0.0f;
inline float joyMagnitude = 0.0f;

inline bool touchDownSent = false;
inline float lastTouchX = 0.0f;
inline float lastTouchY = 0.0f;
inline bool wasTargeting = false; 

inline Vector2 targetScreenPos;
inline bool hasTarget = false;
inline uintptr_t currentTargetAddr = 0;

struct TargetInfo {
uintptr_t address;
Vector3 worldPos;
Vector2 screenPos;
float distanceToPlayer;
float distanceToCenter;
int health;
int maxHP;
bool isPlayer;
bool isValid;

TargetInfo() : address(0), distanceToPlayer(0), distanceToCenter(0), 
health(0), maxHP(0), isPlayer(false), isValid(false) {}

float getHPPercent() const {
return maxHP > 0 ? (float)health / (float)maxHP : 0.0f;
}    };
}

static float CalcScreenDist(float x1, float y1, float x2, float y2) {
const float dx = x2 - x1;
const float dy = y2 - y1;
return std::sqrt(dx * dx + dy * dy);
}
static float LerpAngle(float from, float to, float t) {
float diff = to - from;
while (diff >  M_PI) diff -= 2.0f * M_PI;
while (diff < -M_PI) diff += 2.0f * M_PI;
return from + diff * t;
}

namespace MobaTouch {
static void SendTouchDown(float x, float y) {
if (AutoAim::touchDownSent) return; 

AutoAim::lastTouchX = x;
AutoAim::lastTouchY = y;
Touch_Down(x, y);
AutoAim::touchDownSent = true;
AutoAim::wasTargeting = true;
}

static void SendTouchMove(float x, float y) {
if (!AutoAim::touchDownSent) {
SendTouchDown(x, y);
return;
}
AutoAim::wasTargeting = true;
AutoAim::lastTouchX = x;
AutoAim::lastTouchY = y;
Touch_Move(x, y);
}

static void SendTouchUp() {
if (!AutoAim::touchDownSent) return; 

Touch_Up();
AutoAim::touchDownSent = false;
AutoAim::wasTargeting = false;
}

static void Reset() {
AutoAim::touchDownSent = false;
AutoAim::wasTargeting = false;
}
}

namespace TestMode {
static void Update(float deltaTime) {
if (!AutoAim::testMode) {
return;
}
AutoAim::testAngle += AutoAim::testSpeed * deltaTime;
if (AutoAim::testAngle > 2.0f * M_PI) {
AutoAim::testAngle -= 2.0f * M_PI;
}

const float radius = AutoAim::joyRadius - 25.0f;
const float testX = AutoAim::joyCenterX + std::cos(AutoAim::testAngle) * radius;
const float testY = AutoAim::joyCenterY + std::sin(AutoAim::testAngle) * radius;

MobaTouch::SendTouchMove(testX, testY);
}

static void Draw(ImDrawList* draw) {
if (!AutoAim::testMode) return;

const float radius = AutoAim::joyRadius - 25.0f;
const float dotX = AutoAim::joyCenterX + std::cos(AutoAim::testAngle) * radius;
const float dotY = AutoAim::joyCenterY + std::sin(AutoAim::testAngle) * radius;

draw->AddCircle(ImVec2(AutoAim::joyCenterX, AutoAim::joyCenterY), radius, IM_COL32(140, 80, 255, 100), 64, 1.0f);
draw->AddCircleFilled(ImVec2(dotX, dotY), 12.0f, IM_COL32(140, 80, 255, 220));

char statusText[64];
snprintf(statusText, sizeof(statusText), "TEST: %.1f rad/s", AutoAim::testSpeed);
draw->AddText(ImVec2(AutoAim::joyCenterX - 50.0f, AutoAim::joyCenterY - AutoAim::joyRadius - 30.0f), IM_COL32(140, 80, 255, 255), statusText);
}
static void Start() { AutoAim::testMode = true; AutoAim::testAngle = 0.0f; }
static void Stop() { AutoAim::testMode = false; MobaTouch::SendTouchUp(); }
}

namespace AutoAim {
static bool CollectTargets(AutoAim::TargetInfo* outTargets, size_t maxTargets, 
uintptr_t selfAddr, long a32, long libbase) {
if (!selfAddr) return false;

Vector3 myPos;
vm_readv(selfAddr + OFF_SE(vCachePosition), &myPos, sizeof(myPos));
const float screenCX = (float)abs_ScreenX * 0.5f;
const float screenCY = (float)abs_ScreenY * 0.5f;

size_t idx = 0;

const long playerBase = ReadPtr(ReadPtr(a32 + OFF_BM(ShowPlayers)) + OFF_BM(ListDataOffset)) + OFF_BM(ListArrayOffset);
const uint playerCount = Read<uint>(ReadPtr(a32 + OFF_BM(ShowPlayers)) + OFF_BM(ListCountOffset));

for (uint i = 0; i < playerCount && idx < maxTargets; ++i) {
const uintptr_t objAddr = ReadPtr(playerBase + (i << 3));
if (!objAddr) continue;
if (Read<bool>(objAddr + OFF_SE(bSameCampType))) continue;
if (Read<bool>(objAddr + OFF_SE(bDeath))) continue;            
const int hp = Read<int>(objAddr + OFF_SE(Hp));
const int maxHp = Read<int>(objAddr + OFF_SE(HpMax));
if (hp <= 0 || maxHp <= 0) continue;

Vector3 entityPos;
vm_readv(objAddr + OFF_SE(vCachePosition), &entityPos, sizeof(entityPos));

Vector2 screenPos;
if (!WorldToScreen(entityPos, &screenPos)) continue;
if (screenPos.X < 0 || screenPos.X > abs_ScreenX || 
screenPos.Y < 0 || screenPos.Y > abs_ScreenY) continue;

outTargets[idx].address = objAddr;
outTargets[idx].worldPos = entityPos;
outTargets[idx].screenPos = screenPos;
outTargets[idx].distanceToPlayer = Vector3::Distance(myPos, entityPos);
outTargets[idx].distanceToCenter = CalcScreenDist(screenCX, screenCY, screenPos.X, screenPos.Y);
outTargets[idx].health = hp;
outTargets[idx].maxHP = maxHp;
outTargets[idx].isPlayer = true;
outTargets[idx].isValid = true;
++idx;
}

const long monsterBase = ReadPtr(ReadPtr(a32 + OFF_BM(ShowMonsters)) + OFF_BM(ListDataOffset)) + OFF_BM(ListArrayOffset);
const uint monsterCount = Read<uint>(ReadPtr(a32 + OFF_BM(ShowMonsters)) + OFF_BM(ListCountOffset));

for (uint i = 0; i < monsterCount && idx < maxTargets; ++i) {
const uintptr_t objAddr = ReadPtr(monsterBase + (i << 3));
if (!objAddr) continue;
const int mType = Read<int>(objAddr + OFF_SE(iType));
if (mType != 2 && mType != 5) continue;
const int mHeroID = Read<int>(objAddr + OFF_SE(HeroID));
if (!bMonster(mHeroID) && mHeroID != 2002 && mHeroID != 2003) continue;

if (Read<bool>(objAddr + OFF_SE(bSameCampType))) continue;
if (Read<bool>(objAddr + OFF_SE(bDeath))) continue;

const int hp = Read<int>(objAddr + OFF_SE(Hp));
const int maxHp = Read<int>(objAddr + OFF_SE(HpMax));
if (hp <= 0 || maxHp <= 0) continue;

Vector3 entityPos;
vm_readv(objAddr + OFF_SE(vCachePosition), &entityPos, sizeof(entityPos));

Vector2 screenPos;
if (!WorldToScreen(entityPos, &screenPos)) continue;
if (screenPos.X < 0 || screenPos.X > abs_ScreenX || 
screenPos.Y < 0 || screenPos.Y > abs_ScreenY) continue;
outTargets[idx].address = objAddr;
outTargets[idx].worldPos = entityPos;
outTargets[idx].screenPos = screenPos;
outTargets[idx].distanceToPlayer = Vector3::Distance(myPos, entityPos);
outTargets[idx].distanceToCenter = CalcScreenDist(screenCX, screenCY, screenPos.X, screenPos.Y);
outTargets[idx].health = hp;
outTargets[idx].maxHP = maxHp;
outTargets[idx].isPlayer = false;
outTargets[idx].isValid = true;
++idx;
}
return idx > 0;
}

static AutoAim::TargetInfo* SelectBestTarget(AutoAim::TargetInfo* candidates, size_t count) {
AutoAim::TargetInfo* best = nullptr;
for (size_t i = 0; i < count; ++i) {
if (!candidates[i].isValid) continue;
if (candidates[i].distanceToCenter > fovRadius) continue;
if (!best) { best = &candidates[i]; continue; }
switch (priorityMode) {
case 0: if (candidates[i].distanceToCenter < best->distanceToCenter) best = &candidates[i]; break;
case 1: if (candidates[i].getHPPercent() < best->getHPPercent()) best = &candidates[i]; break;
case 2: if (candidates[i].isPlayer && !best->isPlayer) best = &candidates[i];
else if (candidates[i].isPlayer == best->isPlayer && candidates[i].distanceToCenter < best->distanceToCenter) best = &candidates[i]; break;
}
}
return best;
}
}

namespace Joystick {
static void HandleInput(float touchX, float touchY, bool isDown) {
if (AutoAim::testMode || AutoAim::enabled) return; 

if (!AutoAim::joyVisible) { 
AutoAim::joyActive = false; 
MobaTouch::SendTouchUp();
return; 
}

const float dx = touchX - AutoAim::joyCenterX;
const float dy = touchY - AutoAim::joyCenterY;
const float dist = std::sqrt(dx * dx + dy * dy);

if (isDown) {
if (!AutoAim::joyDragging && dist < AutoAim::joyRadius + 20.0f) {
AutoAim::joyDragging = true;
}            if (AutoAim::joyDragging) {
AutoAim::joyActive = true;
const float clampedDist = std::fmin(dist, AutoAim::joyRadius);
AutoAim::joyMagnitude = (clampedDist - AutoAim::joyDeadZone) / (AutoAim::joyRadius - AutoAim::joyDeadZone);
AutoAim::joyAngle = std::atan2(dy, dx);

const float moveX = AutoAim::joyCenterX + std::cos(AutoAim::joyAngle) * clampedDist;
const float moveY = AutoAim::joyCenterY + std::sin(AutoAim::joyAngle) * clampedDist;

MobaTouch::SendTouchMove(moveX, moveY);
}
} else {
AutoAim::joyDragging = false;
AutoAim::joyActive = false;
AutoAim::joyMagnitude = 0.0f;
MobaTouch::SendTouchUp();        
}
}

static void Draw(ImDrawList* draw) {
if (!AutoAim::joyVisible) return;

draw->AddCircleFilled(ImVec2(AutoAim::joyCenterX, AutoAim::joyCenterY), AutoAim::joyRadius + 8.0f, IM_COL32(15, 10, 35, 120));
draw->AddCircle(ImVec2(AutoAim::joyCenterX, AutoAim::joyCenterY), AutoAim::joyRadius, IM_COL32(140, 80, 255, 180), 32, 2.5f);

if (AutoAim::joyActive && !AutoAim::testMode && !AutoAim::enabled) {
const float knobDist = AutoAim::joyMagnitude * (AutoAim::joyRadius - 25.0f);
const float knobX = AutoAim::joyCenterX + std::cos(AutoAim::joyAngle) * knobDist;
const float knobY = AutoAim::joyCenterY + std::sin(AutoAim::joyAngle) * knobDist;
const float knobSize = 25.0f + AutoAim::joyMagnitude * 10.0f;

draw->AddCircleFilled(ImVec2(knobX, knobY), knobSize, IM_COL32(140, 100, 255, 220));
draw->AddCircle(ImVec2(knobX, knobY), knobSize, IM_COL32(180, 140, 255, 255), 20, 1.5f);
} else if (!AutoAim::testMode && !AutoAim::enabled) {
draw->AddCircleFilled(ImVec2(AutoAim::joyCenterX, AutoAim::joyCenterY), 25.0f, IM_COL32(100, 50, 200, 180));
}
}
}

namespace AutoAim {
static void DrawFOV(ImDrawList* draw) {
if (!enabled || !showVisuals) return;

const float centerX = (float)abs_ScreenX * 0.5f;
const float centerY = (float)abs_ScreenY * 0.5f;

ImU32 fovColor = hasTarget ? IM_COL32(255, 50, 150, 200) : IM_COL32(0, 220, 255, 120);
draw->AddCircle(ImVec2(centerX, centerY), fovRadius, fovColor, 100, 2.0f);

const float crossSize = 10.0f;        draw->AddLine(ImVec2(centerX - crossSize, centerY), ImVec2(centerX + crossSize, centerY), IM_COL32(255, 255, 255, 255), 1.5f);
draw->AddLine(ImVec2(centerX, centerY - crossSize), ImVec2(centerX, centerY + crossSize), IM_COL32(255, 255, 255, 255), 1.5f);

if (hasTarget && !testMode) {
draw->AddLine(ImVec2(centerX, centerY), ImVec2(targetScreenPos.X, targetScreenPos.Y), IM_COL32(255, 50, 150, 220), 1.8f);

draw->AddCircle(ImVec2(targetScreenPos.X, targetScreenPos.Y), 35.0f, IM_COL32(255, 50, 150, 255), 12, 2.5f);

draw->AddCircleFilled(ImVec2(targetScreenPos.X, targetScreenPos.Y), 5.0f, IM_COL32(255, 255, 255, 255));
}
}
}

static void UpdateAutoAim(uintptr_t selfAddr, long a32, long libbase, float deltaTime) {
if (AutoAim::testMode) return;

if (!AutoAim::enabled) {
if (AutoAim::wasTargeting) MobaTouch::SendTouchUp();
AutoAim::hasTarget = false;
return;
}

Vector3 myPos;
vm_readv(selfAddr + OFF_SE(vCachePosition), &myPos, sizeof(myPos));

int CampType = Read<int>(selfAddr + OFF_SP(m_EntityCampType));

AutoAim::TargetInfo candidates[40];
memset(candidates, 0, sizeof(candidates));

if (AutoAim::CollectTargets(candidates, 40, selfAddr, a32, libbase)) {
AutoAim::TargetInfo* selected = AutoAim::SelectBestTarget(candidates, 40);

if (selected) {
AutoAim::hasTarget = true;
AutoAim::currentTargetAddr = selected->address;

float diffX = selected->worldPos.X - myPos.X;
float diffZ = selected->worldPos.Z - myPos.Z;

if (CampType == 2) {
diffX = -diffX;
diffZ = -diffZ;
}

float targetAngle = std::atan2(-diffZ, diffX);

const float cameraRotationOffset = -0.7854f; 
targetAngle += cameraRotationOffset;
float dynamicSmooth = AutoAim::smoothFactor * (deltaTime * 60.0f);
if (dynamicSmooth > 1.0f) dynamicSmooth = 1.0f;

AutoAim::joyAngle = LerpAngle(AutoAim::joyAngle, targetAngle, dynamicSmooth);

const float moveDist = AutoAim::joyRadius - 2.0f;
const float finalX = AutoAim::joyCenterX + std::cos(AutoAim::joyAngle) * moveDist;
const float finalY = AutoAim::joyCenterY + std::sin(AutoAim::joyAngle) * moveDist;

MobaTouch::SendTouchMove(finalX, finalY);

} else {
AutoAim::hasTarget = false;
if (AutoAim::wasTargeting) MobaTouch::SendTouchUp();
}
} else {
AutoAim::hasTarget = false;
if (AutoAim::wasTargeting) MobaTouch::SendTouchUp();
}
}
/* Room Info code removed */

/* ===================================================== */
/* THEME SYSTEM (3 presets) + Shadow + ApplyStyle        */
/* ===================================================== */
namespace Theme {
inline int current = 0; /* 0=Cyan Neon, 1=Matrix Green, 2=Royal Gold */
inline const char* names[] = { "Black & White", "White Red" };

struct Colors {
/* Core palette (ImU32) */
ImU32 accent, accentDim, highlight, danger, text, textDim, bg, bgPanel;
/* Extended UI colors */
ImU32 border, separator, shadowCol;
ImU32 btnBg, btnHover, btnActive;
ImU32 checkMark, sliderGrab, sliderGrabAct;
ImU32 frameBg, frameBgHov, frameBgAct;
ImU32 headerBg, headerHov, headerAct;
ImU32 scrollBg, scrollGrab, scrollGrabHov, scrollGrabAct;
ImU32 teamBlue, teamRed;
/* Core palette (ImVec4) */
ImVec4 accentV, dangerV, textV, textDimV, highlightV;
ImVec4 bgV, bgPanelV, borderV, btnBgV, btnHoverV, btnActiveV;
ImVec4 warningV, successV, infoV;
};

inline Colors presets[] = {
{ /* ── Black & White (Hitam Putih) ── */
/* accent, accentDim, highlight, danger, text, textDim, bg, bgPanel */
IM_COL32(255, 255, 255, 255),  IM_COL32(180, 180, 180, 180),
IM_COL32(255, 255, 255, 255),  IM_COL32(220, 60, 60, 255),
IM_COL32(240, 240, 240, 255),  IM_COL32(120, 120, 120, 200),
IM_COL32(8, 8, 8, 252),        IM_COL32(18, 18, 18, 248),
/* border, separator, shadowCol */
IM_COL32(80, 80, 80, 80),      IM_COL32(70, 70, 70, 60),
IM_COL32(0, 0, 0, 0),
/* btnBg, btnHover, btnActive */
IM_COL32(60, 60, 60, 200),     IM_COL32(90, 90, 90, 240),
IM_COL32(45, 45, 45, 255),
/* checkMark, sliderGrab, sliderGrabAct */
IM_COL32(255, 255, 255, 255),  IM_COL32(200, 200, 200, 230),
IM_COL32(255, 255, 255, 255),
/* frameBg, frameBgHov, frameBgAct */
IM_COL32(22, 22, 22, 200),     IM_COL32(35, 35, 35, 220),
IM_COL32(18, 18, 18, 240),
/* headerBg, headerHov, headerAct */
IM_COL32(50, 50, 50, 120),     IM_COL32(70, 70, 70, 160),
IM_COL32(40, 40, 40, 200),
/* scrollBg, scrollGrab, scrollGrabHov, scrollGrabAct */
IM_COL32(12, 12, 12, 200),     IM_COL32(80, 80, 80, 160),
IM_COL32(110, 110, 110, 200),  IM_COL32(140, 140, 140, 240),
/* teamBlue, teamRed */
IM_COL32(14, 14, 20, 230),     IM_COL32(20, 14, 14, 230),
/* ImVec4: accentV, dangerV, textV, textDimV, highlightV */
ImVec4(1.0f,1.0f,1.0f,1.0f),    ImVec4(0.86f,0.24f,0.24f,1.0f),
ImVec4(0.94f,0.94f,0.94f,1.0f), ImVec4(0.47f,0.47f,0.47f,1.0f),
ImVec4(1.0f,1.0f,1.0f,1.0f),
/* bgV, bgPanelV, borderV, btnBgV, btnHoverV, btnActiveV */
ImVec4(0.031f,0.031f,0.031f,0.99f), ImVec4(0.071f,0.071f,0.071f,0.97f),
ImVec4(0.31f,0.31f,0.31f,0.31f),
ImVec4(0.24f,0.24f,0.24f,0.78f),  ImVec4(0.35f,0.35f,0.35f,0.94f),
ImVec4(0.18f,0.18f,0.18f,1.0f),
/* warningV, successV, infoV */
ImVec4(1.0f,0.75f,0.2f,1.0f),   ImVec4(0.3f,0.9f,0.5f,1.0f),
ImVec4(0.7f,0.7f,0.7f,1.0f)
},
{ /* ── White Red (Putih + Merah) ── */
/* accent, accentDim, highlight, danger, text, textDim, bg, bgPanel */
IM_COL32(200, 30, 45, 255),   IM_COL32(160, 25, 40, 180),
IM_COL32(230, 50, 60, 255),   IM_COL32(220, 40, 40, 255),
IM_COL32(30, 25, 25, 255),    IM_COL32(100, 95, 90, 200),
IM_COL32(245, 242, 240, 252), IM_COL32(235, 230, 228, 248),
/* border, separator, shadowCol */
IM_COL32(200, 180, 175, 80),  IM_COL32(210, 190, 185, 60),
IM_COL32(180, 160, 155, 50),
/* btnBg, btnHover, btnActive */
IM_COL32(200, 30, 45, 200),   IM_COL32(220, 50, 60, 240),
IM_COL32(180, 25, 40, 255),
/* checkMark, sliderGrab, sliderGrabAct */
IM_COL32(210, 35, 50, 255),   IM_COL32(190, 30, 45, 230),
IM_COL32(210, 35, 50, 255),
/* frameBg, frameBgHov, frameBgAct */
IM_COL32(228, 222, 220, 200), IM_COL32(218, 212, 210, 220),
IM_COL32(232, 226, 224, 240),
/* headerBg, headerHov, headerAct */
IM_COL32(200, 30, 45, 50),    IM_COL32(200, 30, 45, 80),
IM_COL32(200, 30, 45, 110),
/* scrollBg, scrollGrab, scrollGrabHov, scrollGrabAct */
IM_COL32(230, 225, 222, 200), IM_COL32(200, 30, 45, 130),
IM_COL32(210, 40, 55, 170),   IM_COL32(220, 50, 60, 210),
/* teamBlue, teamRed */
IM_COL32(230, 235, 245, 230), IM_COL32(248, 230, 230, 230),
/* ImVec4: accentV, dangerV, textV, textDimV, highlightV */
ImVec4(0.78f,0.12f,0.18f,1.0f), ImVec4(0.86f,0.16f,0.16f,1.0f),
ImVec4(0.12f,0.1f,0.1f,1.0f),   ImVec4(0.39f,0.37f,0.35f,1.0f),
ImVec4(0.9f,0.2f,0.24f,1.0f),
/* bgV, bgPanelV, borderV, btnBgV, btnHoverV, btnActiveV */
ImVec4(0.96f,0.95f,0.94f,0.99f), ImVec4(0.92f,0.9f,0.89f,0.97f),
ImVec4(0.78f,0.71f,0.69f,0.31f),
ImVec4(0.78f,0.12f,0.18f,0.78f), ImVec4(0.86f,0.2f,0.24f,0.94f),
ImVec4(0.71f,0.1f,0.16f,1.0f),
/* warningV, successV, infoV */
ImVec4(0.9f,0.45f,0.0f,1.0f),  ImVec4(0.1f,0.7f,0.3f,1.0f),
ImVec4(0.78f,0.12f,0.18f,1.0f)
}
};

inline Colors& C() { return presets[current]; }

/* Apply full ImGui style from current theme */
inline void ApplyImGuiStyle() {
auto& c = C();
ImGuiStyle& s = ImGui::GetStyle();

/* Rounding & spacing */
s.WindowRounding   = 10.0f;
s.ChildRounding    = 8.0f;
s.FrameRounding    = 5.0f;
s.GrabRounding     = 4.0f;
s.ScrollbarRounding= 6.0f;
s.TabRounding      = 5.0f;
s.PopupRounding    = 6.0f;
s.WindowBorderSize = 1.0f;
s.ChildBorderSize  = 1.0f;
s.FrameBorderSize  = 0.0f;
s.ItemSpacing      = ImVec2(8, 6);
s.ItemInnerSpacing = ImVec2(6, 4);
s.ScrollbarSize    = 22.0f;  /* Bigger scrollbar — easier to touch on mobile */
s.GrabMinSize      = 20.0f;  /* Minimum grab (thumb) size */

/* Colors */
ImVec4* cl = s.Colors;
cl[ImGuiCol_WindowBg]           = c.bgV;
cl[ImGuiCol_ChildBg]            = ImVec4(0,0,0,0);
cl[ImGuiCol_PopupBg]            = c.bgPanelV;
cl[ImGuiCol_Border]             = c.borderV;
cl[ImGuiCol_BorderShadow]       = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); /* shadows disabled */
cl[ImGuiCol_FrameBg]            = ImVec4(((c.frameBg>>0)&0xFF)/255.f, ((c.frameBg>>8)&0xFF)/255.f, ((c.frameBg>>16)&0xFF)/255.f, ((c.frameBg>>24)&0xFF)/255.f);
cl[ImGuiCol_FrameBgHovered]     = ImVec4(((c.frameBgHov>>0)&0xFF)/255.f, ((c.frameBgHov>>8)&0xFF)/255.f, ((c.frameBgHov>>16)&0xFF)/255.f, ((c.frameBgHov>>24)&0xFF)/255.f);
cl[ImGuiCol_FrameBgActive]      = ImVec4(((c.frameBgAct>>0)&0xFF)/255.f, ((c.frameBgAct>>8)&0xFF)/255.f, ((c.frameBgAct>>16)&0xFF)/255.f, ((c.frameBgAct>>24)&0xFF)/255.f);
cl[ImGuiCol_TitleBg]            = c.bgPanelV;
cl[ImGuiCol_TitleBgActive]      = c.bgPanelV;
cl[ImGuiCol_TitleBgCollapsed]   = c.bgPanelV;
cl[ImGuiCol_MenuBarBg]          = c.bgPanelV;
cl[ImGuiCol_ScrollbarBg]        = ImVec4(((c.scrollBg>>0)&0xFF)/255.f, ((c.scrollBg>>8)&0xFF)/255.f, ((c.scrollBg>>16)&0xFF)/255.f, ((c.scrollBg>>24)&0xFF)/255.f);
cl[ImGuiCol_ScrollbarGrab]      = ImVec4(((c.scrollGrab>>0)&0xFF)/255.f, ((c.scrollGrab>>8)&0xFF)/255.f, ((c.scrollGrab>>16)&0xFF)/255.f, ((c.scrollGrab>>24)&0xFF)/255.f);
cl[ImGuiCol_ScrollbarGrabHovered]= ImVec4(((c.scrollGrabHov>>0)&0xFF)/255.f, ((c.scrollGrabHov>>8)&0xFF)/255.f, ((c.scrollGrabHov>>16)&0xFF)/255.f, ((c.scrollGrabHov>>24)&0xFF)/255.f);
cl[ImGuiCol_ScrollbarGrabActive]= ImVec4(((c.scrollGrabAct>>0)&0xFF)/255.f, ((c.scrollGrabAct>>8)&0xFF)/255.f, ((c.scrollGrabAct>>16)&0xFF)/255.f, ((c.scrollGrabAct>>24)&0xFF)/255.f);
cl[ImGuiCol_CheckMark]          = c.accentV;
cl[ImGuiCol_SliderGrab]         = ImVec4(((c.sliderGrab>>0)&0xFF)/255.f, ((c.sliderGrab>>8)&0xFF)/255.f, ((c.sliderGrab>>16)&0xFF)/255.f, ((c.sliderGrab>>24)&0xFF)/255.f);
cl[ImGuiCol_SliderGrabActive]   = ImVec4(((c.sliderGrabAct>>0)&0xFF)/255.f, ((c.sliderGrabAct>>8)&0xFF)/255.f, ((c.sliderGrabAct>>16)&0xFF)/255.f, ((c.sliderGrabAct>>24)&0xFF)/255.f);
cl[ImGuiCol_Button]             = c.btnBgV;
cl[ImGuiCol_ButtonHovered]      = c.btnHoverV;
cl[ImGuiCol_ButtonActive]       = c.btnActiveV;
cl[ImGuiCol_Header]             = ImVec4(((c.headerBg>>0)&0xFF)/255.f, ((c.headerBg>>8)&0xFF)/255.f, ((c.headerBg>>16)&0xFF)/255.f, ((c.headerBg>>24)&0xFF)/255.f);
cl[ImGuiCol_HeaderHovered]      = ImVec4(((c.headerHov>>0)&0xFF)/255.f, ((c.headerHov>>8)&0xFF)/255.f, ((c.headerHov>>16)&0xFF)/255.f, ((c.headerHov>>24)&0xFF)/255.f);
cl[ImGuiCol_HeaderActive]       = ImVec4(((c.headerAct>>0)&0xFF)/255.f, ((c.headerAct>>8)&0xFF)/255.f, ((c.headerAct>>16)&0xFF)/255.f, ((c.headerAct>>24)&0xFF)/255.f);
cl[ImGuiCol_Separator]          = c.borderV;
cl[ImGuiCol_SeparatorHovered]   = c.accentV;
cl[ImGuiCol_SeparatorActive]    = c.accentV;
cl[ImGuiCol_Tab]                = c.bgPanelV;
cl[ImGuiCol_TabHovered]         = c.btnHoverV;
cl[ImGuiCol_Text]               = c.textV;
cl[ImGuiCol_TextDisabled]       = c.textDimV;
}

/* DrawPanelShadow — disabled (shadows removed) */
inline void DrawPanelShadow(ImDrawList* draw, ImVec2 p0, ImVec2 p1, float radius = 12.0f) {
(void)draw; (void)p0; (void)p1; (void)radius;
}
}

/* GlowText — shadows removed, direct text only */
inline void GlowText(ImDrawList* draw, float x, float y, ImU32 col, const char* txt) {
draw->AddText(ImVec2(x, y), col, txt);
}

/* ShadowTextColored — shadows removed, plain colored text */
inline void ShadowTextColored(const ImVec4& col, const char* fmt, ...) {
va_list args;
va_start(args, fmt);
char buf[512];
vsnprintf(buf, sizeof(buf), fmt, args);
va_end(args);

/* Direct text — no shadow layers */
ImGui::TextColored(col, "%s", buf);
}


/* RenderPlayerTable, GetTeamChildHeight, RenderRoomPlayerInfoImGui removed */

struct LoginServerData {
uint64_t AccountId;
std::string ClientRealVersion;
std::string ClientStreamVersion;
std::string ClientLocalVersion;
std::string ClientLoginVersion;
std::string Channel;
bool IsNewAccount;
uint32_t RegionId;
std::string CountryInfo;
std::string CountryInfoByIP;
std::string InternetOperator;
std::string State;
uint32_t AccCreateTime;
uint32_t AccountFlag;
std::string UserIP;
std::string GameServerIP;
int32_t GameServerPort;
std::string EngineBuildVersion;
bool IsValid;

LoginServerData() : AccountId(0), IsNewAccount(false), RegionId(0),
AccCreateTime(0), AccountFlag(0), GameServerPort(0), IsValid(false) {}
};

LoginServerData ReadLoginServerInfo(uintptr_t libbase) {
LoginServerData data{};

auto uiRegionBase = Read<uintptr_t>(libbase + OFF_UIREG(BasePtr));
if (!uiRegionBase) return data;

auto ptrChain1 = Read<uintptr_t>(uiRegionBase + OFF_UIREG(PtrChain1));
if (!ptrChain1) return data;

auto loginInfoPtr = Read<uintptr_t>(ptrChain1 + OFF_UIREG(LoginServerInfo));
if (!loginInfoPtr) return data;

data.AccountId              = Read<uint64_t>(loginInfoPtr + 0x28);
data.ClientRealVersion      = fshy(Read<uintptr_t>(loginInfoPtr + 0x30));
data.ClientStreamVersion    = fshy(Read<uintptr_t>(loginInfoPtr + 0x38));
data.ClientLocalVersion     = fshy(Read<uintptr_t>(loginInfoPtr + 0x40));
data.ClientLoginVersion     = fshy(Read<uintptr_t>(loginInfoPtr + 0x50));
data.Channel                = fshy(Read<uintptr_t>(loginInfoPtr + 0x58));
data.IsNewAccount           = Read<bool>(loginInfoPtr + 0x60);
data.RegionId               = Read<uint32_t>(loginInfoPtr + 0x64);
data.CountryInfo            = fshy(Read<uintptr_t>(loginInfoPtr + 0x68));
data.CountryInfoByIP        = fshy(Read<uintptr_t>(loginInfoPtr + 0x70));
data.InternetOperator       = fshy(Read<uintptr_t>(loginInfoPtr + 0x78));
data.State                  = fshy(Read<uintptr_t>(loginInfoPtr + 0x80));
data.AccCreateTime          = Read<uint32_t>(loginInfoPtr + 0x88);
data.AccountFlag            = Read<uint32_t>(loginInfoPtr + 0x8c);
data.UserIP                 = fshy(Read<uintptr_t>(loginInfoPtr + 0x90));
data.GameServerIP           = fshy(Read<uintptr_t>(loginInfoPtr + 0xb8));
data.GameServerPort         = Read<int32_t>(loginInfoPtr + 0xc0);
data.EngineBuildVersion     = fshy(Read<uintptr_t>(loginInfoPtr + 0xf8));

data.IsValid = true;
return data;
}
LoginServerData g_LoginServerCache{};
std::time_t g_LoginServerLastUpdate = 0;
constexpr float LOGIN_INFO_CACHE_DURATION = 2.0f;
bool fakerank = false;
bool WriteRankLevel(uint32_t rankID) {
if (!fakerank || !is_attached || !libbase) return false;

auto systemBase = Read<uintptr_t>(libbase + OFF_SD(BasePtr));
if (!systemBase) return false;

auto chainPtr = Read<uintptr_t>(systemBase + OFF_SD(PtrChain1));
if (!chainPtr) return false;

Write<uint32_t>(chainPtr + OFF_SD(m_rankLevelID), rankID);
return true;
}
static int currentRankIndex = 0;
static bool writePending = false;
void RenderRankSpoofCombo() {


ImGui::PushItemWidth(200.0f);

const char* previewLabel = (currentRankIndex <= 136) ? strRank[currentRankIndex].c_str() : RankToString(currentRankIndex, 0).c_str();
ImGui::Text("Fake Rank:"); ImGui::SameLine();
if (ImGui::BeginCombo(oxorany("##RankCombo"), previewLabel)) {
for (int i = 0; i <= 136; i++) {
bool isSelected = (currentRankIndex == i);
if (ImGui::Selectable(strRank[i].c_str(), isSelected)) {
currentRankIndex = i;
writePending = true;
}
if (isSelected) ImGui::SetItemDefaultFocus();
}

ImGui::Separator();
ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.2f, 1.0f), oxorany("Mythic Ranks:"));

for (int star = 1; star <= 150; star++) {
int rankID = 136 + star;
std::string mythLabel = RankToString(rankID, 0);
bool isSelected = (currentRankIndex == rankID);

if (ImGui::Selectable(mythLabel.c_str(), isSelected)) {
currentRankIndex = rankID;
writePending = true;
}
if (isSelected) ImGui::SetItemDefaultFocus();
}


ImGui::EndCombo();
}
ImGui::Checkbox(oxorany("ENABLE FAKE RANK"), &fakerank);
ImGui::PopItemWidth();


}




/* ===================================================== */
/* MACRO - Skill Combo System                            */
/* ===================================================== */
namespace Macro {
inline bool enabled = false;
inline std::atomic<bool> executing{false};
inline int presetIndex = 0;
inline float speedFactor = 1.2f;
inline float triggerSize = 55.0f;
inline float triggerX = 0;
inline float triggerY = 0;
inline bool posInit = false;
inline bool editMode = false;
inline bool autoTab = false;
inline int draggingIdx = -1; /* -1=none, 0=S1, 1=S2, 2=ULT, 3=TAB */
inline float circleRadius = 35.0f;

/* Touch position percentages (adjustable via UI) */
inline float s1XPct = 0.855f, s1YPct = 0.68f;
inline float s2XPct = 0.78f,  s2YPct = 0.84f;
inline float s3XPct = 0.925f, s3YPct = 0.52f;
inline float trigXPct = 0.92f, trigYPct = 0.38f;

/* Skill positions (screen coords) */
inline float s1X = 0, s1Y = 0;
inline float s2X = 0, s2Y = 0;
inline float s3X = 0, s3Y = 0; /* ULT */

struct ComboPreset {
const char* name;
int steps[6];
int count;
};

/* Presets: 1=S1, 2=S2, 3=ULT */
inline ComboPreset presets[] = {
{ "Simple Burst (2-1-3)",       {2,1,3,0,0,0}, 3 },
{ "Poke (1-2)",                 {1,2,0,0,0,0}, 2 },
{ "Full Combo (2-1-3-1-2)",     {2,1,3,1,2,0}, 5 },
{ "Reverse (3-2-1)",            {3,2,1,0,0,0}, 3 },
};
inline const int presetCount = 4;

void RecalcPositions() {
int sX = ::abs_ScreenX;
int sY = ::abs_ScreenY;
s1X = sX * s1XPct;  s1Y = sY * s1YPct;
s2X = sX * s2XPct;  s2Y = sY * s2YPct;
s3X = sX * s3XPct;  s3Y = sY * s3YPct;
triggerX = sX * trigXPct;
triggerY = sY * trigYPct;
}

void InitPositions() {
if (posInit) return;
RecalcPositions();
posInit = true;
}

void ResetPositions() {
s1XPct = 0.855f; s1YPct = 0.68f;
s2XPct = 0.78f;  s2YPct = 0.84f;
s3XPct = 0.925f; s3YPct = 0.52f;
trigXPct = 0.92f; trigYPct = 0.38f;
RecalcPositions();
}

void TapSkill(int skill) {
float x = 0, y = 0;
if (skill == 1) { x = s1X; y = s1Y; }
else if (skill == 2) { x = s2X; y = s2Y; }
else if (skill == 3) { x = s3X; y = s3Y; }
if (x > 0) {
Touch_Down(x, y);
usleep(30000);
Touch_Up();
}
}

void* ComboThread(void*) {
executing = true;
auto& p = presets[presetIndex];
int delayUs = (int)(speedFactor * 100000);
for (int i = 0; i < p.count; i++) {
if (p.steps[i] > 0) TapSkill(p.steps[i]);
if (i < p.count - 1) usleep(delayUs);
}
executing = false;
return nullptr;
}

void Execute() {
if (executing || !enabled) return;
InitPositions();
pthread_t tid;
pthread_create(&tid, nullptr, ComboThread, nullptr);
pthread_detach(tid);
}

inline bool wasTouching = false;

/* Helper: draw a labeled circle */
void DrawLabeledCircle(ImDrawList* draw, ImVec2 pos, float r, ImU32 fill, ImU32 border, const char* label) {
draw->AddCircleFilled(pos, r, fill, 32);
draw->AddCircle(pos, r, border, 32, 3.0f);
auto sz = ImGui::CalcTextSize(label);
draw->AddText(ImVec2(pos.x - sz.x*0.5f, pos.y - sz.y*0.5f), IM_COL32(255,255,255,255), label);
}

/* Get pointer to position by index */
void GetCirclePos(int idx, float*& outX, float*& outY) {
switch(idx) {
case 0: outX = &s1X; outY = &s1Y; break;
case 1: outX = &s2X; outY = &s2Y; break;
case 2: outX = &s3X; outY = &s3Y; break;
case 3: outX = &triggerX; outY = &triggerY; break;
default: outX = nullptr; outY = nullptr;
}
}

void UpdatePctFromCoords() {
int sX = ::abs_ScreenX;
int sY = ::abs_ScreenY;
if (sX > 0 && sY > 0) {
s1XPct = s1X / sX; s1YPct = s1Y / sY;
s2XPct = s2X / sX; s2YPct = s2Y / sY;
s3XPct = s3X / sX; s3YPct = s3Y / sY;
trigXPct = triggerX / sX; trigYPct = triggerY / sY;
}
}

void DrawTriggerButton(ImDrawList* draw) {
if (!enabled) return;
InitPositions();

ImGuiIO& io = ImGui::GetIO();
bool touching = io.MouseDown[0];
float mx = io.MousePos.x;
float my = io.MousePos.y;

if (editMode) {
/* === EDIT MODE: show draggable circles === */

/* Banner */
const char* banner = "EDIT MODE: Drag circles to set button positions";
auto bsz = ImGui::CalcTextSize(banner);
draw->AddRectFilled(ImVec2(0, 0), ImVec2(bsz.x + 30, bsz.y + 16), IM_COL32(200, 30, 30, 220));
draw->AddText(ImVec2(15, 8), IM_COL32(255, 255, 255, 255), banner);

/* Circle info: idx, label, color */
struct CInfo { int idx; const char* label; ImU32 fill; ImU32 border; };
CInfo circles[] = {
{0, "1",   IM_COL32(230, 80, 50, 220),  IM_COL32(255,120,80,255)},  /* S1 - red-orange */
{1, "2",   IM_COL32(50, 200, 80, 220),  IM_COL32(80,255,120,255)},  /* S2 - green */
{2, "ULT", IM_COL32(200, 200, 30, 220), IM_COL32(255,255,80,255)},  /* ULT - yellow */
{3, "TAB", IM_COL32(30, 180, 50, 220),  IM_COL32(60,255,80,255)},   /* TAB/Trigger - bright green */
};

/* Draw & handle drag for each circle */
for (auto& c : circles) {
float *px, *py;
GetCirclePos(c.idx, px, py);
if (!px) continue;
ImVec2 pos(*px, *py);
float r = (c.idx == 3) ? triggerSize * 0.7f : circleRadius;

/* Check if touch is inside this circle */
float dx = mx - pos.x;
float dy = my - pos.y;
bool inside = (dx*dx + dy*dy) <= (r * r);

/* Start dragging */
if (touching && inside && draggingIdx == -1) {
draggingIdx = c.idx;
}

/* Continue dragging */
if (touching && draggingIdx == c.idx) {
*px = mx;
*py = my;
pos = ImVec2(mx, my);
}

/* Draw circle (brighter if dragging) */
ImU32 fillCol = (draggingIdx == c.idx) ? IM_COL32(255,255,100,240) : c.fill;
DrawLabeledCircle(draw, pos, r, fillCol, c.border, c.label);
}

/* Release drag */
if (!touching && draggingIdx >= 0) {
UpdatePctFromCoords();
draggingIdx = -1;
}

/* Also draw GO button (dimmed, non-interactive) */
ImVec2 goPos(triggerX, triggerY);
draw->AddCircleFilled(goPos, triggerSize, IM_COL32(10, 10, 20, 80), 32);
draw->AddCircle(goPos, triggerSize, IM_COL32(0, 180, 255, 80), 32, 2.0f);
auto gsz = ImGui::CalcTextSize("GO");
draw->AddText(ImVec2(goPos.x - gsz.x*0.5f, goPos.y - gsz.y*0.5f), IM_COL32(0,180,255,80), "GO");

} else {
/* === NORMAL MODE: GO button with tap detection === */
ImVec2 pos(triggerX, triggerY);
ImU32 col = executing ? IM_COL32(255, 50, 50, 220) : IM_COL32(0, 180, 255, 200);
draw->AddCircleFilled(pos, triggerSize, IM_COL32(10, 10, 20, 180), 32);
draw->AddCircle(pos, triggerSize, col, 32, 3.0f);
const char* txt = executing ? "..." : "GO";
auto sz = ImGui::CalcTextSize(txt);
draw->AddText(ImVec2(pos.x - sz.x*0.5f, pos.y - sz.y*0.5f), col, txt);

/* Also draw small skill position indicators */
struct SInfo { float x; float y; const char* label; ImU32 col; };
SInfo smarks[] = {
{s1X, s1Y, "1", IM_COL32(230,80,50,180)},
{s2X, s2Y, "2", IM_COL32(50,200,80,180)},
{s3X, s3Y, "3", IM_COL32(200,200,30,180)},
};
for (auto& s : smarks) {
draw->AddCircleFilled(ImVec2(s.x, s.y), 18.0f, s.col, 24);
auto ssz = ImGui::CalcTextSize(s.label);
draw->AddText(ImVec2(s.x - ssz.x*0.5f, s.y - ssz.y*0.5f), IM_COL32(255,255,255,220), s.label);
}

/* Detect tap on GO button */
float dx = mx - triggerX;
float dy = my - triggerY;
bool inCircle = (dx*dx + dy*dy) <= (triggerSize * triggerSize);
if (!touching && wasTouching && inCircle && !executing) {
Execute();
}
wasTouching = touching && inCircle;
}
}

bool IsTriggerTapped(float touchX, float touchY) {
if (!enabled) return false;
float dx = touchX - triggerX;
float dy = touchY - triggerY;
return (dx*dx + dy*dy) <= (triggerSize * triggerSize);
}
}


/* ===================================================== */
/* Retri Floating Toggle (Rectangular, 2 modes)          */
/* 0 = LORD+TURTLE   |   1 = BUFF                       */
/* ===================================================== */
namespace RetriOverlay {
inline float btnX = 0.0f, btnY = 0.0f;
inline float btnW = 160.0f, btnH = 48.0f;
inline bool initialized = false;
inline bool dragging    = false;
inline bool posLocked   = false; /* Lock button position so it can't be dragged */
/* 0 = Lord+Turtle, 1 = Buff */
inline int mode = 0;

void Init() {
if (!initialized && abs_ScreenX > 0) {
btnX = abs_ScreenX * 0.82f;
btnY = abs_ScreenY * 0.42f;
initialized = true;
}
}

void Draw(ImDrawList* draw) {
if (!autoRetribution) return;
Init();

ImGuiIO& io = ImGui::GetIO();
float rounding = 10.0f;

/* Hit test rect */
ImVec2 p0(btnX - btnW * 0.5f, btnY - btnH * 0.5f);
ImVec2 p1(btnX + btnW * 0.5f, btnY + btnH * 0.5f);
bool hovered = (io.MousePos.x >= p0.x && io.MousePos.x <= p1.x &&
io.MousePos.y >= p0.y && io.MousePos.y <= p1.y);

/* Drag to reposition (only when not locked) */
if (io.MouseDown[0] && hovered && !dragging && !posLocked) {
dragging = true;
}
if (dragging) {
if (io.MouseDown[0]) {
btnX = io.MousePos.x;
btnY = io.MousePos.y;
p0 = ImVec2(btnX - btnW * 0.5f, btnY - btnH * 0.5f);
p1 = ImVec2(btnX + btnW * 0.5f, btnY + btnH * 0.5f);
} else {
dragging = false;
}
}

/* Colors based on mode */
const char* label;
ImU32 fillCol, borderCol, textCol;
int sr = (Theme::C().shadowCol >> 0) & 0xFF;
int sg = (Theme::C().shadowCol >> 8) & 0xFF;
int sb = (Theme::C().shadowCol >> 16) & 0xFF;

if (mode == 0) {
label = "BOSS";
fillCol   = IM_COL32(180, 35, 35, 200);
borderCol = IM_COL32(255, 80, 80, 230);
textCol   = IM_COL32(255, 220, 220, 255);
} else {
label = "BUFF";
fillCol   = IM_COL32(20, 120, 50, 200);
borderCol = IM_COL32(80, 255, 120, 230);
textCol   = IM_COL32(200, 255, 215, 255);
}

/* Shadow layers */
draw->AddRectFilled(ImVec2(p0.x-4, p0.y-4), ImVec2(p1.x+4, p1.y+4),
IM_COL32(sr, sg, sb, 35), rounding + 4);
draw->AddRectFilled(ImVec2(p0.x-2, p0.y-2), ImVec2(p1.x+2, p1.y+2),
IM_COL32(sr, sg, sb, 55), rounding + 2);

/* Button body */
draw->AddRectFilled(p0, p1, fillCol, rounding);
draw->AddRect(p0, p1, borderCol, rounding, 0, 2.5f);

/* Text centered with shadow */
auto sz = ImGui::CalcTextSize(label);
float tx = btnX - sz.x * 0.5f;
float ty = btnY - sz.y * 0.5f;
draw->AddText(ImVec2(tx + 1.5f, ty + 1.5f), IM_COL32(0,0,0,180), label);
draw->AddText(ImVec2(tx, ty), textCol, label);

/* Detect tap (not drag) */
static bool wasDown = false;
static float downX = 0, downY = 0;
if (io.MouseDown[0] && !wasDown) {
wasDown = true;
downX = io.MousePos.x;
downY = io.MousePos.y;
} else if (!io.MouseDown[0] && wasDown) {
wasDown = false;
/* Only toggle if didn't drag far */
float dx = io.MousePos.x - downX;
float dy = io.MousePos.y - downY;
if (dx*dx + dy*dy < 100.0f && hovered) {
/* Toggle: Lord+Turtle ↔ Buff */
mode = 1 - mode;
if (mode == 0) {
AutoRetributionLord   = true;
AutoRetributionTurtle = true;
AutoRetributionRed    = false;
AutoRetributionBlue   = false;
} else {
AutoRetributionLord   = false;
AutoRetributionTurtle = false;
AutoRetributionRed    = true;
AutoRetributionBlue   = true;
}
}
}
}
}


/* ===================================================== */
/* Save/Load Settings */
namespace Settings {
inline const char* savePath = "/data/local/tmp/mlbb_settings.cfg";

void Save() {
FILE* f = fopen(savePath, "w");
if (!f) return;
/* === Theme === */
fprintf(f, "theme=%d\n", Theme::current);
/* === ESP === */
fprintf(f, "drawMHealth=%d\n", drawMHealth ? 1 : 0);
fprintf(f, "iconhero=%d\n", iconhero ? 1 : 0);
fprintf(f, "drawMDistance=%d\n", drawMDistance ? 1 : 0);
fprintf(f, "drawAlertUnderAttack=%d\n", drawAlertUnderAttack ? 1 : 0);
fprintf(f, "drawMPrediction=%d\n", drawMPrediction ? 1 : 0);
/* === Drone === */
fprintf(f, "EnableDrone=%d\n", EnableDrone ? 1 : 0);
fprintf(f, "FieldView=%.2f\n", FieldView);
fprintf(f, "EnableDroneV2=%d\n", EnableDroneV2 ? 1 : 0);
fprintf(f, "DroneHeight=%.2f\n", DroneHeight);
/* === Retribution === */
fprintf(f, "autoRetribution=%d\n", autoRetribution ? 1 : 0);
fprintf(f, "retriTouchX=%.1f\n", retriTouchX);
fprintf(f, "retriTouchY=%.1f\n", retriTouchY);
fprintf(f, "AutoRetriRed=%d\n", AutoRetributionRed ? 1 : 0);
fprintf(f, "AutoRetriBlue=%d\n", AutoRetributionBlue ? 1 : 0);
fprintf(f, "AutoRetriLord=%d\n", AutoRetributionLord ? 1 : 0);
fprintf(f, "AutoRetriTurtle=%d\n", AutoRetributionTurtle ? 1 : 0);
fprintf(f, "enableRoomInfo=%d\n", enableRoomInfo ? 1 : 0);
fprintf(f, "RetriMode=%d\n", RetriOverlay::mode);
fprintf(f, "RetriBtnX=%.1f\n", RetriOverlay::btnX);
fprintf(f, "RetriBtnY=%.1f\n", RetriOverlay::btnY);
fprintf(f, "RetriPosLocked=%d\n", RetriOverlay::posLocked ? 1 : 0);
/* === AutoAim === */
fprintf(f, "autoaim=%d\n", AutoAim::enabled ? 1 : 0);
fprintf(f, "fovRadius=%.1f\n", AutoAim::fovRadius);
fprintf(f, "smoothFactor=%.2f\n", AutoAim::smoothFactor);
fprintf(f, "aimPriority=%d\n", AutoAim::priorityMode);
fprintf(f, "aimVisuals=%d\n", AutoAim::showVisuals ? 1 : 0);
fprintf(f, "joyVisible=%d\n", AutoAim::joyVisible ? 1 : 0);
fprintf(f, "joyRadius=%.1f\n", AutoAim::joyRadius);
fprintf(f, "joyDeadZone=%.1f\n", AutoAim::joyDeadZone);
fprintf(f, "joyCenterX=%.1f\n", AutoAim::joyCenterX);
fprintf(f, "joyCenterY=%.1f\n", AutoAim::joyCenterY);
/* === Macro === */
fprintf(f, "macroEnabled=%d\n", Macro::enabled ? 1 : 0);
fprintf(f, "macroPreset=%d\n", Macro::presetIndex);
fprintf(f, "macroSpeed=%.1f\n", Macro::speedFactor);
fprintf(f, "macroAutoTab=%d\n", Macro::autoTab ? 1 : 0);
fprintf(f, "macroTrigSize=%.1f\n", Macro::triggerSize);
fprintf(f, "macroS1X=%.3f\n", Macro::s1XPct);
fprintf(f, "macroS1Y=%.3f\n", Macro::s1YPct);
fprintf(f, "macroS2X=%.3f\n", Macro::s2XPct);
fprintf(f, "macroS2Y=%.3f\n", Macro::s2YPct);
fprintf(f, "macroS3X=%.3f\n", Macro::s3XPct);
fprintf(f, "macroS3Y=%.3f\n", Macro::s3YPct);
fprintf(f, "macroTrigX=%.3f\n", Macro::trigXPct);
fprintf(f, "macroTrigY=%.3f\n", Macro::trigYPct);
/* === Minimap === */
fprintf(f, "MinimapIcon=%d\n", MinimapIcon ? 1 : 0);
fprintf(f, "MinimapSize=%d\n", MinimapSize);
fprintf(f, "MinimapPos=%d\n", MinimapPos);
fprintf(f, "HideLine=%d\n", HideLine ? 1 : 0);
fprintf(f, "g_ICSize=%d\n", g_ICSize);
fprintf(f, "g_MinimapScale=%.2f\n", g_MinimapScale);
fprintf(f, "g_Res0_MultX=%.2f\n", g_Res0_MultX);
fprintf(f, "g_Res0_MultY=%.2f\n", g_Res0_MultY);
fprintf(f, "g_Res1_OffsetX=%.2f\n", g_Res1_OffsetX);
fprintf(f, "g_Res1_OffsetY=%.2f\n", g_Res1_OffsetY);
/* === Rank Spoof === */
fprintf(f, "fakerank=%d\n", fakerank ? 1 : 0);
fprintf(f, "currentRankIndex=%d\n", currentRankIndex);
fclose(f);
}

void Load() {
FILE* f = fopen(savePath, "r");
if (!f) return;
int iv; float fv;
char line[128];
while (fgets(line, sizeof(line), f)) {
/* === Theme === */
if (sscanf(line, "theme=%d", &iv) == 1) Theme::current = (iv >= 0 && iv < 2) ? iv : 0;
/* === ESP === */
if (sscanf(line, "drawMHealth=%d", &iv) == 1) drawMHealth = (iv != 0);
if (sscanf(line, "iconhero=%d", &iv) == 1) iconhero = (iv != 0);
if (sscanf(line, "drawMDistance=%d", &iv) == 1) drawMDistance = (iv != 0);
if (sscanf(line, "drawAlertUnderAttack=%d", &iv) == 1) drawAlertUnderAttack = (iv != 0);
if (sscanf(line, "drawMPrediction=%d", &iv) == 1) drawMPrediction = (iv != 0);
/* === Drone === */
if (sscanf(line, "EnableDrone=%d", &iv) == 1) EnableDrone = (iv != 0);
if (sscanf(line, "FieldView=%f", &fv) == 1) FieldView = fv;
if (sscanf(line, "EnableDroneV2=%d", &iv) == 1) EnableDroneV2 = (iv != 0);
if (sscanf(line, "DroneHeight=%f", &fv) == 1) DroneHeight = fv;
/* === Retribution === */
if (sscanf(line, "autoRetribution=%d", &iv) == 1) autoRetribution = (iv != 0);
if (sscanf(line, "retriTouchX=%f", &fv) == 1) retriTouchX = fv;
if (sscanf(line, "retriTouchY=%f", &fv) == 1) retriTouchY = fv;
if (sscanf(line, "AutoRetriRed=%d", &iv) == 1) AutoRetributionRed = (iv != 0);
if (sscanf(line, "AutoRetriBlue=%d", &iv) == 1) AutoRetributionBlue = (iv != 0);
if (sscanf(line, "AutoRetriLord=%d", &iv) == 1) AutoRetributionLord = (iv != 0);
if (sscanf(line, "AutoRetriTurtle=%d", &iv) == 1) AutoRetributionTurtle = (iv != 0);

if (sscanf(line, "enableRoomInfo=%d", &iv) == 1) enableRoomInfo = (iv != 0);
if (sscanf(line, "RetriMode=%d", &iv) == 1) RetriOverlay::mode = iv;
if (sscanf(line, "RetriBtnX=%f", &fv) == 1) RetriOverlay::btnX = fv;
if (sscanf(line, "RetriBtnY=%f", &fv) == 1) RetriOverlay::btnY = fv;
if (sscanf(line, "RetriPosLocked=%d", &iv) == 1) RetriOverlay::posLocked = (iv != 0);
/* === AutoAim === */
if (sscanf(line, "autoaim=%d", &iv) == 1) AutoAim::enabled = (iv != 0);
if (sscanf(line, "fovRadius=%f", &fv) == 1) AutoAim::fovRadius = fv;
if (sscanf(line, "smoothFactor=%f", &fv) == 1) AutoAim::smoothFactor = fv;
if (sscanf(line, "aimPriority=%d", &iv) == 1) AutoAim::priorityMode = iv;
if (sscanf(line, "aimVisuals=%d", &iv) == 1) AutoAim::showVisuals = (iv != 0);
if (sscanf(line, "joyVisible=%d", &iv) == 1) AutoAim::joyVisible = (iv != 0);
if (sscanf(line, "joyRadius=%f", &fv) == 1) AutoAim::joyRadius = fv;
if (sscanf(line, "joyDeadZone=%f", &fv) == 1) AutoAim::joyDeadZone = fv;
if (sscanf(line, "joyCenterX=%f", &fv) == 1) AutoAim::joyCenterX = fv;
if (sscanf(line, "joyCenterY=%f", &fv) == 1) AutoAim::joyCenterY = fv;
/* === Macro === */
if (sscanf(line, "macroEnabled=%d", &iv) == 1) Macro::enabled = (iv != 0);
if (sscanf(line, "macroPreset=%d", &iv) == 1) Macro::presetIndex = iv;
if (sscanf(line, "macroSpeed=%f", &fv) == 1) Macro::speedFactor = fv;
if (sscanf(line, "macroAutoTab=%d", &iv) == 1) Macro::autoTab = (iv != 0);
if (sscanf(line, "macroTrigSize=%f", &fv) == 1) Macro::triggerSize = fv;
if (sscanf(line, "macroS1X=%f", &fv) == 1) Macro::s1XPct = fv;
if (sscanf(line, "macroS1Y=%f", &fv) == 1) Macro::s1YPct = fv;
if (sscanf(line, "macroS2X=%f", &fv) == 1) Macro::s2XPct = fv;
if (sscanf(line, "macroS2Y=%f", &fv) == 1) Macro::s2YPct = fv;
if (sscanf(line, "macroS3X=%f", &fv) == 1) Macro::s3XPct = fv;
if (sscanf(line, "macroS3Y=%f", &fv) == 1) Macro::s3YPct = fv;
if (sscanf(line, "macroTrigX=%f", &fv) == 1) Macro::trigXPct = fv;
if (sscanf(line, "macroTrigY=%f", &fv) == 1) Macro::trigYPct = fv;
/* === Minimap === */
if (sscanf(line, "MinimapIcon=%d", &iv) == 1) MinimapIcon = (iv != 0);
if (sscanf(line, "MinimapSize=%d", &iv) == 1) MinimapSize = iv;
if (sscanf(line, "MinimapPos=%d", &iv) == 1) MinimapPos = iv;
if (sscanf(line, "HideLine=%d", &iv) == 1) HideLine = (iv != 0);
if (sscanf(line, "g_ICSize=%d", &iv) == 1) g_ICSize = iv;
if (sscanf(line, "g_MinimapScale=%f", &fv) == 1) g_MinimapScale = fv;
if (sscanf(line, "g_Res0_MultX=%f", &fv) == 1) g_Res0_MultX = fv;
if (sscanf(line, "g_Res0_MultY=%f", &fv) == 1) g_Res0_MultY = fv;
if (sscanf(line, "g_Res1_OffsetX=%f", &fv) == 1) g_Res1_OffsetX = fv;
if (sscanf(line, "g_Res1_OffsetY=%f", &fv) == 1) g_Res1_OffsetY = fv;
/* === Rank Spoof === */
if (sscanf(line, "fakerank=%d", &iv) == 1) fakerank = (iv != 0);
if (sscanf(line, "currentRankIndex=%d", &iv) == 1) currentRankIndex = iv;
}
fclose(f);
}
}


void Layout_tick_UI() {
Theme::ApplyImGuiStyle();
if (is_root_mode) {
ImGui::SetNextWindowSize(ImVec2(450, 460), ImGuiCond_FirstUseEver);
ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
if (Memory::IsKernelModeActive()) {
ImGui::Begin(oxorany("ngentod Kernel"));
} else {
ImGui::Begin(oxorany("SNITHEXERC MLBB"));
}

if (!is_attached) {
ShadowTextColored(Theme::C().dangerV, "Game Not Detected");
if (ImGui::Button(oxorany("Open MLBB"), ImVec2(-1, 50))) {
AttachToGame();
}
} else {

/* ========================================================= */
/* SIDEBAR NAVIGATION (SHINE-style)                          */
/* ========================================================= */
static int selectedTab = 0;

struct TabInfo { const char* name; const char* icon; const char* desc; };
TabInfo tabs[] = {
{ "ESP",       "ESP",  "Visual overlays & drone camera" },
{ "Minimap",    "SRV",  "Server info & minimap settings" },
{ "Retri",     "RET",  "Auto retribution config" },
{ "AutoAim",   "AIM",  "Auto aim & joystick" },
{ "Room Info", "INFO", "Room player information" },
{ "Settings",  "SET",  "Theme & app settings" },
};
const int tabCount = 6;

/* Rounding & spacing now handled by Theme::ApplyImGuiStyle() */

float sidebarW = 120.0f;

/* --- LEFT SIDEBAR (with shadow) --- */
{
ImVec2 wp = ImGui::GetCursorScreenPos();
Theme::DrawPanelShadow(ImGui::GetWindowDrawList(),
wp, ImVec2(wp.x + sidebarW, wp.y + ImGui::GetContentRegionAvail().y), 8.0f);
}
ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::C().bgPanel);
ImGui::BeginChild("##sidebar", ImVec2(sidebarW, -1.0f), true);
LoadSidebarIcons();
for (int i = 0; i < tabCount; i++) {
ImGui::PushID(i);
bool isActive = (selectedTab == i);

if (isActive) {
ImGui::PushStyleColor(ImGuiCol_Button, Theme::C().accent);
ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::C().btnHover);
ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::C().btnActive);
} else {
ImGui::PushStyleColor(ImGuiCol_Button, Theme::C().bgPanel);
ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::C().frameBgHov);
ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::C().btnActive);
}

if (g_SidebarTex[i]) {
if (ImGui::ImageButton((ImTextureID)(intptr_t)g_SidebarTex[i], ImVec2(73, 72), ImVec2(0,0), ImVec2(1,1), 4,
isActive ? Theme::C().accentV : Theme::C().bgPanelV)) 
selectedTab = i;
} else {
if (ImGui::Button(tabs[i].icon, ImVec2(-1, 42))) selectedTab = i;
}
ImGui::PopStyleColor(3);
ImGui::PopID();
}
ImGui::EndChild();
ImGui::PopStyleColor();

ImGui::SameLine();

/* --- RIGHT CONTENT PANEL (with shadow) --- */
{
ImVec2 wp = ImGui::GetCursorScreenPos();
ImVec2 avail = ImGui::GetContentRegionAvail();
Theme::DrawPanelShadow(ImGui::GetWindowDrawList(),
wp, ImVec2(wp.x + avail.x, wp.y + avail.y), 10.0f);
}
ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::C().bg);
ImGui::PushStyleColor(ImGuiCol_Border, Theme::C().border);
ImGui::BeginChild("##content", ImVec2(0, -1.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

/* Header */
ShadowTextColored(Theme::C().accentV, "%s", tabs[selectedTab].name);
ImGui::TextColored(Theme::C().textDimV, "%s", tabs[selectedTab].desc);
ImGui::Separator();
ImGui::Spacing();

/* =================== TAB CONTENT =================== */

if (selectedTab == 0) {
/* === ESP === */
ImGui::Checkbox(oxorany("ESP Healt"), &drawMHealth);
ImGui::Checkbox(oxorany("Avatar,"), &iconhero);
ImGui::Checkbox(oxorany("Distance & Hero Name"), &drawMDistance);
ImGui::Checkbox(oxorany("Alert Lord Under Attack"), &drawAlertUnderAttack);
ImGui::Spacing();
ShadowTextColored(Theme::C().accentV, " DRONE CAMERA");
ImGui::Separator();
ImGui::Spacing();
ImGui::Checkbox(oxorany("Drone View V1"), &EnableDrone);
if (EnableDrone) {
ImGui::Indent(10.0f);
ImGui::SliderFloat(oxorany("FOV"), &FieldView, -9.0f, -1.0f, "%.1f");
ImGui::Unindent(10.0f);
}
ImGui::Checkbox(oxorany("Drone View V2"), &EnableDroneV2);
if (EnableDroneV2) {
ImGui::Indent(10.0f);
ImGui::SliderFloat(oxorany("Height"), &DroneHeight, -50.0f, 50.0f, "%.1f");
ImGui::Unindent(10.0f);
}
}
else if (selectedTab == 1) {
/* === Server Info === */
RenderRankSpoofCombo();
ImGui::Separator();
if (ImGui::CollapsingHeader(oxorany("Minimap Setting"), ImGuiTreeNodeFlags_DefaultOpen)) {
ImGui::Checkbox(oxorany("Minimap"), &MinimapIcon);
ImGui::SameLine();
ImGui::Checkbox(oxorany("Hide Line"), &HideLine);
ImGui::SliderInt(oxorany("Minimap Size"), &MinimapSize, 100, 600);
ImGui::SliderInt(oxorany("Minimap Pos X"), &MinimapPos, 0, 800);
ImGui::SliderInt(oxorany("Icon Size"), &g_ICSize, 1, 100);
ImGui::Separator();                    ImGui::SliderFloat(oxorany("Res0 X Mult"), &g_Res0_MultX, 0.1f, 3.0f);
ImGui::SliderFloat(oxorany("Res0 Y Mult"), &g_Res0_MultY, 0.1f, 3.0f);
ImGui::SliderFloat(oxorany("Res1 Offset X"), &g_Res1_OffsetX, -200.0f, 200.0f);
ImGui::SliderFloat(oxorany("Res1 Offset Y"), &g_Res1_OffsetY, -200.0f, 200.0f);
ImGui::SliderFloat(oxorany("Minimap Scale"), &g_MinimapScale, 10.0f, 150.0f);
}
}
else if (selectedTab == 2) {
/* === Retri === */
ImGui::Checkbox(oxorany("Auto Retri"), &autoRetribution);
ImGui::SliderFloat(oxorany("Adjust X"), &retriTouchX, 0.0f, 3000.0f, "%.0f");
ImGui::SliderFloat(oxorany("Adjust Y"), &retriTouchY, 0.0f, 1500.0f, "%.0f");
ImGui::Separator();
/* ====== Monster Target Toggles (unified style) ====== */
ImGui::Spacing();
ShadowTextColored(Theme::C().accentV, "Target Toggles:");
ImGui::Spacing();
{
float fullW = ImGui::GetContentRegionAvail().x;
float halfW = (fullW - 8.0f) * 0.5f;
float btnH  = 44.0f;

/* Helper lambda — draw a styled toggle button */
auto DrawToggle = [&](const char* onLbl, const char* offLbl, bool& state, float w) {
if (state) {
ImGui::PushStyleColor(ImGuiCol_Button,        Theme::C().accent);
ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  Theme::C().btnHover);
ImGui::PushStyleColor(ImGuiCol_ButtonActive,   Theme::C().btnActive);
ImGui::PushStyleColor(ImGuiCol_Text,           ImVec4(1,1,1,1));
} else {
ImGui::PushStyleColor(ImGuiCol_Button,        Theme::C().frameBg);
ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  Theme::C().frameBgHov);
ImGui::PushStyleColor(ImGuiCol_ButtonActive,   Theme::C().frameBgAct);
ImGui::PushStyleColor(ImGuiCol_Text,           Theme::C().textDimV);
}
ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
if (ImGui::Button(state ? onLbl : offLbl, ImVec2(w, btnH)))
state = !state;
ImGui::PopStyleVar();
ImGui::PopStyleColor(4);
};

/* Row 1: LORD + TURTLE */
DrawToggle("LORD  ON", "LORD  OFF", AutoRetributionLord, halfW);
ImGui::SameLine(0, 8.0f);
DrawToggle("TURTLE  ON", "TURTLE  OFF", AutoRetributionTurtle, halfW);

ImGui::Spacing();

/* Row 2: BUFF RED + BUFF BLUE (same style) */
DrawToggle("BUFF RED  ON", "BUFF RED  OFF", AutoRetributionRed, halfW);
ImGui::SameLine(0, 8.0f);
DrawToggle("BUFF BLUE  ON", "BUFF BLUE  OFF", AutoRetributionBlue, halfW);
}

/* ====== Lock BOSS & BUFF button position ====== */
ImGui::Spacing();
ShadowTextColored(Theme::C().accentV, "BOSS & BUFF Button:");
ImGui::Spacing();
{
float lockW = ImGui::GetContentRegionAvail().x;
if (RetriOverlay::posLocked) {
ImGui::PushStyleColor(ImGuiCol_Button,       Theme::C().accent);
ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::C().btnHover);
ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Theme::C().btnActive);
ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0,0,0,1));
} else {
ImGui::PushStyleColor(ImGuiCol_Button,       Theme::C().frameBg);
ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::C().frameBgHov);
ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Theme::C().frameBgAct);
ImGui::PushStyleColor(ImGuiCol_Text,          Theme::C().textDimV);
}
ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
if (ImGui::Button(
RetriOverlay::posLocked ? "BOSS/BUFF POS  LOCKED" : "BOSS/BUFF POS  FREE",
ImVec2(lockW, 44.0f))) {
RetriOverlay::posLocked = !RetriOverlay::posLocked;
}
ImGui::PopStyleVar();
ImGui::PopStyleColor(4);
ImGui::TextColored(Theme::C().textDimV,
RetriOverlay::posLocked
? "Position locked — drag disabled."
: "Drag BOSS/BUFF button on screen to reposition.");
}
}
else if (selectedTab == 3) {
/* === AutoAim (Simplified) === */
ImGui::Checkbox(oxorany("Enable Auto Aim"), &AutoAim::enabled);
if (AutoAim::enabled) {
ImGui::Spacing();
ImGui::SliderFloat(oxorany("FOV Radius"), &AutoAim::fovRadius, 50.0f, 1500.0f, "%.0f");
ImGui::SliderFloat(oxorany("Smooth Factor"), &AutoAim::smoothFactor, 0.01f, 0.5f, "%.2f");
ImGui::Spacing();
const char* priorities[] = {"Closest", "Lowest HP", "Player Priority"};
ImGui::Combo(oxorany("Target Priority"), &AutoAim::priorityMode, priorities, IM_ARRAYSIZE(priorities));
ImGui::Spacing();
ImGui::Checkbox(oxorany("Show Visuals"), &AutoAim::showVisuals);
ImGui::Checkbox(oxorany("Show Joystick"), &AutoAim::joyVisible);

/* Quick Joystick Position Preset */
if (AutoAim::joyVisible) {
ImGui::Spacing();
float bW = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
if (ImGui::Button(oxorany("Joystick: Left"), ImVec2(bW, 36))) {
AutoAim::joyCenterX = abs_ScreenX * 0.25f;
AutoAim::joyCenterY = abs_ScreenY * 0.85f;
}
ImGui::SameLine(0, 8.0f);
if (ImGui::Button(oxorany("Joystick: Right"), ImVec2(bW, 36))) {
AutoAim::joyCenterX = abs_ScreenX * 0.75f;
AutoAim::joyCenterY = abs_ScreenY * 0.85f;
}
}
}

ImGui::Spacing();
ImGui::Separator();
ImGui::TextColored(Theme::C().successV, oxorany("Status: %s"), AutoAim::hasTarget ? "TARGET LOCKED" : "NO TARGET");

/* ====== MACRO SECTION ====== */
ImGui::Spacing(); ImGui::Spacing();
ImGui::Separator();
ShadowTextColored(Theme::C().accentV, "MACRO");
ImGui::Separator();
ImGui::Spacing();

ImGui::Checkbox(oxorany("Enable Macro"), &Macro::enabled);

if (Macro::enabled) {
Macro::InitPositions();

/* Enable Auto Tab + Finish Setup */
ImGui::Checkbox(oxorany("Enable Auto Tab"), &Macro::autoTab);
ImGui::SameLine();
if (Macro::editMode) {
if (ImGui::Button(oxorany("Finish Setup"))) {
Macro::editMode = false;
}
} else {
if (ImGui::Button(oxorany("Edit Positions"))) {
Macro::editMode = true;
}
}

/* Combo Preset */
ImGui::Spacing();
ImGui::TextColored(Theme::C().infoV, "Select Combo Preset:");
if (ImGui::BeginCombo("##MacroPreset", Macro::presets[Macro::presetIndex].name)) {
for (int i = 0; i < Macro::presetCount; i++) {
bool sel = (Macro::presetIndex == i);
if (ImGui::Selectable(Macro::presets[i].name, sel)) Macro::presetIndex = i;
if (sel) ImGui::SetItemDefaultFocus();
}
ImGui::EndCombo();
}

/* Speed Factor */
ImGui::Spacing();
ImGui::TextColored(Theme::C().infoV, "Combo Tempo / Delay:");
ImGui::SliderFloat(oxorany("Speed Factor"), &Macro::speedFactor, 0.3f, 3.0f, "%.1fx");
const char* speedLabel = (Macro::speedFactor < 0.8f) ? "FAST (Low Delay)" :
         (Macro::speedFactor < 1.5f) ? "NORMAL" : "SLOW (High Delay)";
ImGui::TextColored(Theme::C().successV, "Current: %s", speedLabel);
ImGui::TextColored(Theme::C().textDimV, "Higher Value = Slower Execution (More Delay)");

/* Edit Details (collapsible) */
ImGui::Spacing();
if (ImGui::TreeNode("Edit Details")) {
ImGui::Spacing();
ImGui::TextColored(Theme::C().infoV, "Skill Positions (manual):");
ImGui::Spacing();

bool posChanged = false;
ImGui::TextColored(Theme::C().textV, "Skill 1:");
posChanged |= ImGui::SliderFloat(oxorany("S1 X##s1x"), &Macro::s1XPct, 0.0f, 1.0f, "%.3f");
posChanged |= ImGui::SliderFloat(oxorany("S1 Y##s1y"), &Macro::s1YPct, 0.0f, 1.0f, "%.3f");
ImGui::Spacing();
ImGui::TextColored(Theme::C().textV, "Skill 2:");
posChanged |= ImGui::SliderFloat(oxorany("S2 X##s2x"), &Macro::s2XPct, 0.0f, 1.0f, "%.3f");
posChanged |= ImGui::SliderFloat(oxorany("S2 Y##s2y"), &Macro::s2YPct, 0.0f, 1.0f, "%.3f");
ImGui::Spacing();
ImGui::TextColored(Theme::C().textV, "Skill 3 (Skill 3):");
posChanged |= ImGui::SliderFloat(oxorany("S3 X##s3x"), &Macro::s3XPct, 0.0f, 1.0f, "%.3f");
posChanged |= ImGui::SliderFloat(oxorany("S3 Y##s3y"), &Macro::s3YPct, 0.0f, 1.0f, "%.3f");
ImGui::Spacing();
ImGui::TextColored(Theme::C().textV, "Trigger Button:");
posChanged |= ImGui::SliderFloat(oxorany("Trig X##tx"), &Macro::trigXPct, 0.0f, 1.0f, "%.3f");
posChanged |= ImGui::SliderFloat(oxorany("Trig Y##ty"), &Macro::trigYPct, 0.0f, 1.0f, "%.3f");

if (posChanged) Macro::RecalcPositions();

ImGui::Spacing();
if (ImGui::Button(oxorany("Reset Positions to Default"), ImVec2(-1, 50))) {
Macro::ResetPositions();
}
ImGui::TreePop();
}

/* Trigger Settings (always visible) */
ImGui::Spacing();
ImGui::Text("Trigger Settings:");
ImGui::SliderFloat(oxorany("Trigger Size##main"), &Macro::triggerSize, 30.0f, 200.0f, "%.0f");

/* Instructions */
ImGui::Spacing();
ShadowTextColored(Theme::C().warningV, "Instructions:");
ImGui::BulletText("Drag 'TAB' circle to move trigger.");
ImGui::BulletText("Drag numbered circles to set skill buttons.");
ImGui::BulletText("Tap 'GO' button on screen to execute.");
}
}
else if (selectedTab == 4) {
/* === Room Info === */
ImGui::TextDisabled("Show player info during match loading / pick phase.");
ImGui::Checkbox(oxorany("Enable Room Info"), &enableRoomInfo);
ImGui::Spacing();
ImGuiTableFlags tblFlg = ImGuiTableFlags_Borders
   | ImGuiTableFlags_RowBg
   | ImGuiTableFlags_SizingStretchProp;

ShadowTextColored(Theme::C().accentV, "BLUE TEAM (Ally)");
if (ImGui::BeginTable("##BlueTeam", 6, tblFlg)) {
ImGui::TableSetupColumn("Nickname", ImGuiTableColumnFlags_None, 3.0f);
ImGui::TableSetupColumn("User ID",  ImGuiTableColumnFlags_None, 2.5f);
ImGui::TableSetupColumn("Rank",     ImGuiTableColumnFlags_None, 2.5f);
ImGui::TableSetupColumn("Hero",     ImGuiTableColumnFlags_None, 2.0f);
ImGui::TableSetupColumn("Spell",    ImGuiTableColumnFlags_None, 1.5f);
ImGui::TableSetupColumn("Type",     ImGuiTableColumnFlags_None, 1.5f);
ImGui::TableHeadersRow();
for (int ri = 0; ri < roomInfoBlueCount; ri++) {
ImGui::TableNextRow();
ImGui::TableNextColumn(); ImGui::Text("%s", PlayerB[ri].Name.c_str());
ImGui::TableNextColumn(); ImGui::Text("%s", PlayerB[ri].UserID.c_str());
ImGui::TableNextColumn(); ImGui::Text("%s", PlayerB[ri].Rank.c_str());
ImGui::TableNextColumn(); ImGui::Text("%s", PlayerB[ri].Hero.c_str());
ImGui::TableNextColumn();
ImTextureID btex = GetSpellTexture(PlayerB[ri].SpellID);
if (btex) { ImGui::Image(btex, ImVec2(24,24)); if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",PlayerB[ri].Spell.c_str()); }
else       ImGui::Text("%s", PlayerB[ri].Spell.c_str());
ImGui::TableNextColumn();
if (PlayerB[ri].isBot) ImGui::TextColored(ImVec4(1.f,0.5f,0.2f,1.f),"Bot");
else                   ImGui::TextColored(ImVec4(0.3f,1.f,0.3f,1.f),"Player");
}
if (roomInfoBlueCount == 0) {
ImGui::TableNextRow(); ImGui::TableNextColumn();
ImGui::TextDisabled("No data — enable above & enter loading screen");
}
ImGui::EndTable();
}

ImGui::Spacing();
ShadowTextColored(Theme::C().dangerV, "RED TEAM (Enemy)");
if (ImGui::BeginTable("##RedTeam", 6, tblFlg)) {
ImGui::TableSetupColumn("Nickname", ImGuiTableColumnFlags_None, 3.0f);
ImGui::TableSetupColumn("User ID",  ImGuiTableColumnFlags_None, 2.5f);
ImGui::TableSetupColumn("Rank",     ImGuiTableColumnFlags_None, 2.5f);
ImGui::TableSetupColumn("Hero",     ImGuiTableColumnFlags_None, 2.0f);
ImGui::TableSetupColumn("Spell",    ImGuiTableColumnFlags_None, 1.5f);
ImGui::TableSetupColumn("Type",     ImGuiTableColumnFlags_None, 1.5f);
ImGui::TableHeadersRow();
for (int ri = 0; ri < roomInfoRedCount; ri++) {
ImGui::TableNextRow();
ImGui::TableNextColumn(); ImGui::Text("%s", PlayerR[ri].Name.c_str());
ImGui::TableNextColumn(); ImGui::Text("%s", PlayerR[ri].UserID.c_str());
ImGui::TableNextColumn(); ImGui::Text("%s", PlayerR[ri].Rank.c_str());
ImGui::TableNextColumn(); ImGui::Text("%s", PlayerR[ri].Hero.c_str());
ImGui::TableNextColumn();
ImTextureID rtex = GetSpellTexture(PlayerR[ri].SpellID);
if (rtex) { ImGui::Image(rtex, ImVec2(24,24)); if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",PlayerR[ri].Spell.c_str()); }
else       ImGui::Text("%s", PlayerR[ri].Spell.c_str());
ImGui::TableNextColumn();
if (PlayerR[ri].isBot) ImGui::TextColored(ImVec4(1.f,0.5f,0.2f,1.f),"Bot");
else                   ImGui::TextColored(ImVec4(0.3f,1.f,0.3f,1.f),"Player");
}
if (roomInfoRedCount == 0) {
ImGui::TableNextRow(); ImGui::TableNextColumn();
ImGui::TextDisabled("No data — enable above & enter loading screen");
}
ImGui::EndTable();
}
}
else if (selectedTab == 5) {
/* === Settings === */
/* Theme Selector */
ShadowTextColored(Theme::C().accentV, "Color Theme:");
if (ImGui::Combo(oxorany("##ThemeSelect"), &Theme::current, Theme::names, 2)) {}

ImGui::Spacing();
static float opacity = 1.0f;
ImGui::SliderFloat(oxorany("UI Opacity"), &opacity, 0.1f, 1.0f);
ImGui::GetStyle().Alpha = opacity;

/* Save / Load */
ImGui::Spacing();
ImGui::Separator();
ShadowTextColored(Theme::C().accentV, "Save / Load:");
if (ImGui::Button(oxorany("Save Settings"), ImVec2(-1, 50))) {
Settings::Save();
}
if (ImGui::Button(oxorany("Load Settings"), ImVec2(-1, 50))) {
Settings::Load();
}

ImGui::Spacing();
ImGui::Separator();
ImGui::Spacing();
{
int dr = (int)((Theme::C().danger >>  0) & 0xFF);
int dg = (int)((Theme::C().danger >>  8) & 0xFF);
int db = (int)((Theme::C().danger >> 16) & 0xFF);
ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(dr, dg, db, 220));
ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  IM_COL32(dr+30>255?255:dr+30, dg, db, 240));
ImGui::PushStyleColor(ImGuiCol_ButtonActive,   IM_COL32(dr-30<0?0:dr-30, dg, db, 255));
}
if (ImGui::Button(oxorany("Exit Cheat (Stop Loop)"), ImVec2(-1, 50))) {
main_thread_flag = false;
}
ImGui::Spacing();
if (ImGui::Button(oxorany("Unload Cheat (Terminate)"), ImVec2(-1, 50))) {
exit(0);
}
ImGui::PopStyleColor(3);
}

ImGui::EndChild();
ImGui::PopStyleColor(2); /* content bg + border */
}

g_window = ImGui::GetCurrentWindow();
ImGui::End();
}
if (MinimapIcon) {
DrawMinimapESP(ImGui::GetForegroundDrawList());
}
Macro::DrawTriggerButton(ImGui::GetForegroundDrawList());
RetriOverlay::Draw(ImGui::GetForegroundDrawList());
DrawMonster(ImGui::GetForegroundDrawList());
}

__attribute__((visibility("default"))) void* pid_monitor(void*) {
while (main_thread_flag) {
pid_t pid = pidof(oxorany("com.mobile.legends:UnityKillsMe"));
if (pid == 0) {
is_attached = false;
if (!is_root_mode) AttachToGame();
if (!CHKEXP()) break;
}
usleep(500000); 
}
return nullptr;
}

__attribute__((visibility("default"))) void* game_worker(void*) {
auto lastUpdate = std::chrono::steady_clock::now();

while (main_thread_flag) {
auto currentTime = std::chrono::steady_clock::now();
auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
currentTime - lastUpdate).count();

if (elapsedMs >= 10) {

MonsterRetribution();
CheckAndTriggerRetribution();
/* Room Info — update player list during loading/pick phase */
if (enableRoomInfo && is_attached) {
g_iRoomInfoList();
} else if (!enableRoomInfo) {
roomInfoBlueCount = 0;
roomInfoRedCount  = 0;
}

if (AutoAim::enabled && is_attached && !AutoAim::testMode) {
long a1 = ReadPtr(libbase + OFF_BM(BasePtr));
long a2 = ReadPtr(a1 + OFF_BM(PtrChain1));
long a32 = ReadPtr(a2);
uintptr_t selfAddr = ReadPtr(a32 + OFF_BM(LocalPlayerShow));
UpdateAutoAim(selfAddr, a32, libbase, elapsedMs / 1000.0f);
}
if (writePending) {
if (WriteRankLevel(static_cast<uint32_t>(currentRankIndex))) {
writePending = false;
}
}

lastUpdate = currentTime;
}
std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
return nullptr;
}
#include <curl/curl.h>

/* VPN Detection removed */

/* ======================================================= */
/* Online Key Validation via GitHub JSON                    */
/* ======================================================= */
static size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
((std::string*)userp)->append((char*)contents, size * nmemb);
return size * nmemb;
}

bool validateLoginKeyOnline(const char* key) {
CURL* curl = curl_easy_init();
if (!curl) {
printf("Error: Cannot initialize connection\n");
return false;
}

std::string response;
curl_easy_setopt(curl, CURLOPT_URL, "https://raw.githubusercontent.com/snithexerc/db.json/refs/heads/main/db.json");
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

CURLcode res = curl_easy_perform(curl);
curl_easy_cleanup(curl);

if (res != CURLE_OK) {
printf("Failed to connect to server!\n");
return false;
}

/* Search for key as top-level entry: "keyname": { */
std::string searchKey = "\"" + std::string(key) + "\"";
size_t pos = response.find(searchKey);
if (pos == std::string::npos) {
return false;
}

/* Check expires field after the key position */
size_t expiresPos = response.find("\"expires\"", pos);
if (expiresPos != std::string::npos) {
size_t dateStart = response.find("\"", expiresPos + 9);
if (dateStart != std::string::npos) {
dateStart++;
size_t dateEnd = response.find("\"", dateStart);
if (dateEnd != std::string::npos) {
std::string expiresStr = response.substr(dateStart, dateEnd - dateStart);
/* Parse ISO date: "2026-11-16T17:21:21Z" */
struct tm expTm = {};
int y, mo, d, h, mi, s;
if (sscanf(expiresStr.c_str(), "%d-%d-%dT%d:%d:%dZ", &y, &mo, &d, &h, &mi, &s) == 6) {
expTm.tm_year = y - 1900;
expTm.tm_mon  = mo - 1;
expTm.tm_mday = d;
expTm.tm_hour = h;
expTm.tm_min  = mi;
expTm.tm_sec  = s;
time_t expTime = mktime(&expTm);
time_t now = time(nullptr);
if (difftime(now, expTime) >= 0) {
printf("Key expired!\n");
return false;
}
}
}
}
}

return true;
}

__attribute__((visibility("default"))) int main(int argc, char *argv[]) {
is_root_mode = true;



/* ---- Login Key Validation ---- */
char loginKey[64] = {0};
printf("Login Key : ");
fflush(stdout);
if (fgets(loginKey, sizeof(loginKey), stdin)) {
loginKey[strcspn(loginKey, "\n")] = 0;
}

printf("Validating key...\n");
if (!validateLoginKeyOnline(loginKey)) {
printf("Invalid Key!\n");
return -1;
}
printf("Key Valid! Starting...\n");

Memory::SetReadMode(Memory::ReadMode::Userspace);

if (!CHKEXP()) return -1;
if (!is_attached) AttachToGame();

screen_config();
::abs_ScreenX = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
::abs_ScreenY = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);
::native_window_screen_x = ::abs_ScreenX;
::native_window_screen_y = ::abs_ScreenY;

if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true)) return -1;

Touch_Init(displayInfo.width, displayInfo.height, displayInfo.orientation, false);
/* WindowRounding now handled by Theme::ApplyImGuiStyle() */
Settings::Load();

if (AutoAim::joyCenterX == 0 && AutoAim::joyCenterY == 0) {
AutoAim::joyCenterX = ::abs_ScreenX * 0.25f;
AutoAim::joyCenterY = ::abs_ScreenY * 0.85f;
}

pthread_t pid_tid, worker_tid;
pthread_create(&pid_tid, nullptr, pid_monitor, nullptr);
pthread_create(&worker_tid, nullptr, game_worker, nullptr);

pthread_detach(pid_tid);
pthread_detach(worker_tid);

/* Room Info removed */

struct timespec lastTime;
clock_gettime(CLOCK_MONOTONIC, &lastTime);

while (main_thread_flag) {
drawBegin();

struct timespec currentTime;
clock_gettime(CLOCK_MONOTONIC, &currentTime);
float deltaTime = (currentTime.tv_sec - lastTime.tv_sec) +
(currentTime.tv_nsec - lastTime.tv_nsec) / 1e9f;
lastTime = currentTime;

TestMode::Update(deltaTime);

ImDrawList* draw = ImGui::GetForegroundDrawList();
if (is_root_mode) {
AutoAim::DrawFOV(draw);
TestMode::Draw(draw);
Joystick::Draw(draw);
}

Layout_tick_UI();
drawEnd();
usleep(1000); 
}

TestMode::Stop();
MobaTouch::SendTouchUp();

/* Room Info cleanup removed */

shutdown();
Touch_Close();
return 0;
}
