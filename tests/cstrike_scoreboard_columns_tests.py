"""Exercise cstrike scoreboard optional column expansion on the first refresh receiving health or money.

Run: python -I tests/cstrike_scoreboard_columns_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <map>
#include <string>
#include <vector>

#ifndef _WIN32
#include <strings.h>
#define _snprintf snprintf
#define _snwprintf swprintf
#define stricmp strcasecmp
#endif

#define MAX_PLAYERS 64
#define FCVAR_ARCHIVE 1

struct cvar_s {
 const char* name = "";
 float value = 0.0f;
};

struct Color {
 int r = 0, g = 0, b = 0, a = 0;
 Color() = default;
 Color(int r_, int g_, int b_, int a_) : r(r_), g(g_), b(b_), a(a_) {}
};

struct KeyValues {
 std::map<std::string, int> ints;
 std::map<std::string, std::string> strings;
 std::map<std::string, std::wstring> wstrings;

 explicit KeyValues(const char*) {}
 void SetInt(const char* key, int value) { ints[key] = value; }
 void SetString(const char* key, const char* value) { strings[key] = value ? value : ""; }
 void SetWString(const char* key, const wchar_t* value) { wstrings[key] = value ? value : L""; }
 const char* GetString(const char* key, const char* fallback = "") const {
  auto it = strings.find(key);
  return it != strings.end() ? it->second.c_str() : fallback;
 }
 int GetInt(const char* key, int fallback = 0) const {
  auto it = ints.find(key);
  return it != ints.end() ? it->second : fallback;
 }
 bool HasKey(const char* key) const {
  return strings.count(key) || ints.count(key) || wstrings.count(key);
 }
 void deleteThis() { delete this; }
};

struct extra_player_info_t {
 short frags = 0;
 short deaths = 0;
 int has_c4 = 0;
 int vip = 0;
 short teamnumber = 0;
 bool dead = false;
 int sb_health = -1;
 int sb_account = -1;
 int has_defuse_kit = 0;
};

struct hud_player_info_t {
 const char* name = nullptr;
 short ping = 0;
};

struct cl_entity_t {
 int index = 1;
};

struct VGuiLibraryPlayer_t {
 const char* name = nullptr;
 bool thisplayer = false;
 int teamnumber = 0;
};

struct ScoreBoardTeamInfo {
 int frags = 0;
};

static extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS + 1];
static hud_player_info_t g_PlayerInfoList[MAX_PLAYERS + 1];
static VGuiLibraryPlayer_t g_TestPlayers[MAX_PLAYERS + 1];
static int g_TestMaxPlayers = 4;
static int g_iTeamNumber = 1;
static cl_entity_t g_LocalPlayer;

static cvar_s g_CvarShowHealth = { "scoreboard_showhealth", 1.0f };
static cvar_s g_CvarShowMoney = { "scoreboard_showmoney", 1.0f };
static cvar_s g_CvarShortHeaders = { "scoreboard_shortheaders", 0.0f };
static cvar_s g_CvarShowAvatars = { "scoreboard_showavatars", 1.0f };

struct EngineFuncs {
 cvar_s* pfnGetCvarPointer(const char* name) {
  if (!strcmp(name, "scoreboard_showhealth")) return &g_CvarShowHealth;
  if (!strcmp(name, "scoreboard_showmoney")) return &g_CvarShowMoney;
  if (!strcmp(name, "scoreboard_shortheaders")) return &g_CvarShortHeaders;
  if (!strcmp(name, "scoreboard_showavatars")) return &g_CvarShowAvatars;
  return nullptr;
 }
 cvar_s* pfnRegisterVariable(const char* name, const char*, int) { return pfnGetCvarPointer(name); }
 float pfnGetCvarFloat(const char*) { return 0.0f; }
 const char* PlayerInfo_ValueForKey(int, const char*) { return nullptr; }
 cl_entity_t* GetLocalPlayer() { return &g_LocalPlayer; }
} gEngfuncs;

struct ClientDll {
 int GetMaxPlayers() { return g_TestMaxPlayers; }
 VGuiLibraryPlayer_t CallEnghudGetPlayerInfo(int player) { return g_TestPlayers[player]; }
};

struct ViewPort {
 ClientDll client;
 ClientDll* GetClientDllInterface() { return &client; }
 void MakeSafeName(const char* oldName, char* newName, int capacity) {
  if (!oldName || capacity <= 0) return;
  strncpy(newName, oldName, (size_t)capacity - 1);
  newName[capacity - 1] = 0;
 }
 bool IsSpectatorGUIVisible() { return false; }
} g_ViewPort, *gViewPortInterface = &g_ViewPort;

struct ICareerTask {
 const wchar_t* GetLocalizedTaskName() { return nullptr; }
 bool IsComplete() { return false; }
 int GetRepeat() { return 0; }
 int GetPartial() { return 0; }
 bool IsMaxRoundTime() { return false; }
 bool MustSurvive() { return false; }
};

using ITaskVec = std::vector<ICareerTask*>;
using ITaskVecIt = ITaskVec::iterator;

struct ICareerUI {
 bool IsPlayingMatch() { return false; }
 ITaskVec* GetCurrentTaskVec() { return nullptr; }
 float GetRoundTimeLength() { return 0.0f; }
};

static ICareerUI* g_pCareerUI = nullptr;

struct RoundTimer { float m_flTimeEnd = 0.0f; };
struct HudStub { RoundTimer m_roundTimer; float m_flTime = 0.0f; } gHUD;

static void BufWPrintf(wchar_t* buffer, int&, const wchar_t*, ...) {
 if (buffer) buffer[0] = 0;
}

namespace vgui2 {
using HFont = int;
using HScheme = int;

struct IScheme {
 HFont GetFont(const char*, bool) { return 1; }
};

struct ISchemeManager {
 IScheme scheme;
 IScheme* GetIScheme(HScheme) { return &scheme; }
 int GetProportionalScaledValue(int value) { return value; }
};
static ISchemeManager g_SchemeManager;
static ISchemeManager* scheme() { return &g_SchemeManager; }

struct ISurface {
 void GetScreenSize(int& wide, int& tall) { wide = 1024; tall = 768; }
};
static ISurface g_Surface;
static ISurface* surface() { return &g_Surface; }

struct ILocalize {
 const wchar_t* Find(const char* token) {
  if (!strcmp(token, "#CStrike_HEALTH")) return L"Health";
  if (!strcmp(token, "#CStrike_ACCOUNT")) return L"Money";
  if (!strcmp(token, "#PlayerScore")) return L"Score";
  if (!strcmp(token, "#PlayerDeath")) return L"Deaths";
  if (!strcmp(token, "#PlayerPing")) return L"Latency";
  return L"";
 }
 void ConvertANSIToUnicode(const char* ansi, wchar_t* unicode, int bufferSizeInBytes) {
  if (!unicode || bufferSizeInBytes <= 0) return;
  size_t limit = (size_t)bufferSizeInBytes / sizeof(wchar_t) - 1;
  size_t i = 0;
  for (; ansi && ansi[i] && i < limit; ++i)
   unicode[i] = (wchar_t)(unsigned char)ansi[i];
  unicode[i] = 0;
 }
 void ConstructString(wchar_t* unicodeOutput, int, const wchar_t*, int, ...) {
  if (unicodeOutput) unicodeOutput[0] = 0;
 }
};
static ILocalize g_Localize;
static ILocalize* localize() { return &g_Localize; }

struct Label {
 void SetText(const wchar_t*) {}
 void SizeToContents() {}
 void GetSize(int& w, int& h) { w = 0; h = 0; }
};

struct Column {
 std::string name;
 std::wstring header;
 int flags = 0;
 int width = 0;
};

struct Section {
 int id = 0;
 bool alwaysVisible = false;
 std::vector<Column> columns;
};

struct RowRecord {
 bool valid = true;
 int section = 0;
 KeyValues data{ "data" };
 int healthColumnWidthAtWrite = -1;
 int accountColumnWidthAtWrite = -1;
};

class SectionedListPanel {
public:
 enum {
  COLUMN_RIGHT = 0x01,
  COLUMN_IMAGE = 0x04,
 };

 std::vector<Section> sections;
 std::vector<RowRecord> rows;
 int selectedItem = -1;
 int lineSpacing = 0;

 void Reset() {
  sections.clear();
  rows.clear();
  selectedItem = -1;
  lineSpacing = 0;
 }

 Section* FindSection(int id) {
  for (auto& s : sections)
   if (s.id == id)
    return &s;
  return nullptr;
 }

 Column* FindColumn(int sectionID, const char* colName) {
  Section* s = FindSection(sectionID);
  if (!s) return nullptr;
  for (auto& c : s->columns)
   if (!stricmp(c.name.c_str(), colName))
    return &c;
  return nullptr;
 }

 int GetColumnWidth(int sectionID, const char* colName) {
  Column* c = FindColumn(sectionID, colName);
  assert(c != nullptr);
  return c->width;
 }

 std::wstring GetColumnHeader(int sectionID, const char* colName) {
  Column* c = FindColumn(sectionID, colName);
  assert(c != nullptr);
  return c->header;
 }

 void AddSection(int id, const char*, void*) {
  sections.push_back(Section{ id, false, {} });
 }

 void SetSectionAlwaysVisible(int id, bool visible) {
  if (Section* s = FindSection(id))
   s->alwaysVisible = visible;
 }

 void SetSectionFgColor(int, Color) {}

 bool AddColumnToSection(int id, const char* colName, const wchar_t* text, int flags, int width, HFont = 0) {
  Section* s = FindSection(id);
  if (!s) return false;
  s->columns.push_back(Column{ colName, text ? text : L"", flags, width });
  return true;
 }

 bool AddColumnToSection(int id, const char* colName, const char* text, int flags, int width, HFont = 0) {
  std::wstring wide;
  if (text)
   for (const char* p = text; *p; ++p)
    wide.push_back((wchar_t)(unsigned char)*p);
  return AddColumnToSection(id, colName, wide.c_str(), flags, width);
 }

 bool ModifyColumn(int id, const char* colName, const wchar_t* text, int width) {
  Column* c = FindColumn(id, colName);
  if (!c) return false;
  if (text || c->width != width) {
   if (width >= 0)
    c->width = width;
   if (text)
    c->header = text;
  }
  return true;
 }

 void OverrideLineSpacing(int spacing) { lineSpacing = spacing; }

 int AddItem(int section, const KeyValues* data) {
  RowRecord record;
  record.valid = true;
  record.section = section;
  record.data = *data;
  record.healthColumnWidthAtWrite = FindColumn(section, "health") ? GetColumnWidth(section, "health") : -1;
  record.accountColumnWidthAtWrite = FindColumn(section, "account") ? GetColumnWidth(section, "account") : -1;
  rows.push_back(record);
  return (int)rows.size() - 1;
 }

 bool ModifyItem(int item, int section, const KeyValues* data) {
  if (item < 0 || item >= (int)rows.size() || !rows[item].valid)
   return false;
  rows[item].section = section;
  rows[item].data = *data;
  rows[item].healthColumnWidthAtWrite = FindColumn(section, "health") ? GetColumnWidth(section, "health") : -1;
  rows[item].accountColumnWidthAtWrite = FindColumn(section, "account") ? GetColumnWidth(section, "account") : -1;
  return true;
 }

 bool RemoveItem(int item) {
  if (item < 0 || item >= (int)rows.size() || !rows[item].valid)
   return false;
  rows[item].valid = false;
  return true;
 }

 void SetItemFgColor(int, Color) {}
 void SetSelectedItem(int item) { selectedItem = item; }
};
} // namespace vgui2

class CCSClientScoreBoardDialog {
public:
 enum {
  CSTRIKE_AVATAR_WIDTH = 56,
  CSTRIKE_NAME_WIDTH = 209,
  CSTRIKE_SPACER1_WIDTH = 5,
  CSTRIKE_CLASS_WIDTH = 56,
  CSTRIKE_HEALTH_WIDTH = 41,
  CSTRIKE_ACCOUNT_WIDTH = 57,
  CSTRIKE_SCORE_WIDTH = 45,
  CSTRIKE_DEATH_WIDTH = 46,
  CSTRIKE_PING_WIDTH = 50,
  CSTRIKE_MIN_WIDTH = 38,
  CSTRIKE_MAX_WIDTH = 509
 };
 enum { CSTRIKE_AVATAR_HEIGHT = 32 };

 vgui2::SectionedListPanel m_list;
 vgui2::SectionedListPanel* m_pPlayerList = &m_list;
 vgui2::Label m_taskLabel;
 vgui2::Label* m_pTaskLabel = &m_taskLabel;
 ScoreBoardTeamInfo m_TeamInfo[4];
 int m_iSectionId = 0;
 int m_iPlayersToShow = 0;
 cvar_s* m_pShowHealth = nullptr;
 bool m_bHealthColumnEnabled = false;
 bool m_bHealthUpdateReceived = false;
 cvar_s* m_pShowMoney = nullptr;
 bool m_bMoneyColumnEnabled = false;
 bool m_bMoneyUpdateReceived = false;
 cvar_s* m_pShortHeaders = nullptr;
 bool m_bShortHeaders = false;
 cvar_s* m_pShowAvatars = nullptr;
 bool m_bAvatarColumnEnabled = false;
 int m_nAvatarColumnWidth = 0;

 // Observed inside UpdatePlayerAvatar during UpdatePlayerInfo
 std::vector<bool> m_avatarEnabledDuringRowFill;

 static bool StaticPlayerSortFunc(vgui2::SectionedListPanel*, int, int) { return false; }
 vgui2::HScheme GetScheme() { return 0; }
 Color GetTeamColor(int) { return Color(255, 255, 255, 255); }

 int FindItemIDForPlayerIndex(int playerIndex) {
  for (int i = 0; i < (int)m_list.rows.size(); ++i)
   if (m_list.rows[i].valid && m_list.rows[i].data.GetInt("playerIndex", 0) == playerIndex)
    return i;
  return -1;
 }

 void UpdatePlayerAvatar(int, KeyValues* kv) {
  m_avatarEnabledDuringRowFill.push_back(m_bAvatarColumnEnabled);
  if (!m_bAvatarColumnEnabled || !kv)
   return;
  kv->SetInt("avatar", 1);
 }

 void Reset() {
  m_list.Reset();
  m_avatarEnabledDuringRowFill.clear();
  InitScoreboardSections();
 }

 const wchar_t* GetHealthHeader();
 const wchar_t* GetMoneyHeader();
 const wchar_t* GetScoreHeader();
 const wchar_t* GetDeathHeader();
 const wchar_t* GetPingHeader();
 int GetHeaderWidthNoScale(int maximum, int minimum);
 int GetHeaderWidth(int maximum, int minimum);
 int GetNameHeaderWidth();
 int GetAvatarHeightForScreenHeight(int height);
 int GetAvatarHeaderWidth();
 void InitScoreboardSections();
 void AddHeader();
 void AddSection(int teamType, int teamNumber);
 void UpdateColumnInAllSections(const char* column, const wchar_t* header, int width, bool spectator);
 void UpdateHeaders();
 void UpdateOptionalColumns();
 bool GetPlayerScoreInfo(int playerIndex, KeyValues* kv);
 void UpdatePlayerInfo();
};
'''

TESTS = r'''
static void ResetTestWorld() {
 g_CvarShowHealth.value = 1.0f;
 g_CvarShowMoney.value = 1.0f;
 g_CvarShortHeaders.value = 0.0f;
 g_CvarShowAvatars.value = 1.0f;
 g_iTeamNumber = 1;
 g_pCareerUI = nullptr;
 g_TestMaxPlayers = 4;
 for (int i = 0; i <= MAX_PLAYERS; ++i) {
  g_PlayerExtraInfo[i] = extra_player_info_t{};
  g_PlayerInfoList[i] = hud_player_info_t{};
  g_TestPlayers[i] = VGuiLibraryPlayer_t{};
 }
}

static void SetPlayer(int index, const char* name, int team, bool dead, int health, int account, bool isLocal = false) {
 g_TestPlayers[index] = { name, isLocal, team };
 g_PlayerInfoList[index] = { name, 20 };
 g_PlayerExtraInfo[index].teamnumber = (short)team;
 g_PlayerExtraInfo[index].dead = dead;
 g_PlayerExtraInfo[index].sb_health = health;
 g_PlayerExtraInfo[index].sb_account = account;
}

static void FirstRefreshOpensHealthAndMoneyColumns() {
 ResetTestWorld();
 CCSClientScoreBoardDialog dialog;
 dialog.Reset();

 // Initial widths before any player health/money values are observed
 assert(0 == dialog.m_list.GetColumnWidth(0, "health"));
 assert(0 == dialog.m_list.GetColumnWidth(1, "health"));
 assert(0 == dialog.m_list.GetColumnWidth(2, "health"));
 assert(0 == dialog.m_list.GetColumnWidth(0, "account"));
 assert(0 == dialog.m_list.GetColumnWidth(1, "account"));
 assert(0 == dialog.m_list.GetColumnWidth(2, "account"));
 assert(L"" == dialog.m_list.GetColumnHeader(0, "health"));
 assert(L"" == dialog.m_list.GetColumnHeader(0, "account"));

 SetPlayer(1, "Alice", 1, false, 100, 800, true);
 dialog.UpdatePlayerInfo();

 // Both columns must already be widened before rows are written and remain widened after return
 assert(1u == dialog.m_list.rows.size());
 assert(std::string("100") == dialog.m_list.rows[0].data.GetString("health", ""));
 assert(std::string("$800") == dialog.m_list.rows[0].data.GetString("account", ""));
 assert(CCSClientScoreBoardDialog::CSTRIKE_HEALTH_WIDTH == dialog.m_list.rows[0].healthColumnWidthAtWrite);
 assert(CCSClientScoreBoardDialog::CSTRIKE_ACCOUNT_WIDTH == dialog.m_list.rows[0].accountColumnWidthAtWrite);

 for (int section = 0; section <= 2; ++section) {
  assert(CCSClientScoreBoardDialog::CSTRIKE_HEALTH_WIDTH == dialog.m_list.GetColumnWidth(section, "health"));
  assert(CCSClientScoreBoardDialog::CSTRIKE_ACCOUNT_WIDTH == dialog.m_list.GetColumnWidth(section, "account"));
  assert(dialog.GetNameHeaderWidth() == dialog.m_list.GetColumnWidth(section, "name"));
 }
 assert(std::wstring(L"Health") == dialog.m_list.GetColumnHeader(0, "health"));
 assert(std::wstring(L"Money") == dialog.m_list.GetColumnHeader(0, "account"));
}

static void DeadUnassignedOrNegativeValuesKeepColumnsCollapsed() {
 ResetTestWorld();
 CCSClientScoreBoardDialog dialog;
 dialog.Reset();

 SetPlayer(1, "DeadT", 1, true, 100, -1, true);
 SetPlayer(2, "Spec", 0, false, 100, 800, false);
 SetPlayer(3, "HiddenCT", 2, false, -1, -1, false);
 dialog.UpdatePlayerInfo();

 for (int section = 0; section <= 2; ++section) {
  assert(0 == dialog.m_list.GetColumnWidth(section, "health"));
  assert(0 == dialog.m_list.GetColumnWidth(section, "account"));
 }
 assert(!dialog.m_list.rows[0].data.HasKey("health"));
 assert(!dialog.m_list.rows[0].data.HasKey("account"));

 // Dead team player with visible account opens only account on that same refresh
 g_PlayerExtraInfo[1].sb_account = 1600;
 dialog.UpdatePlayerInfo();
 assert(0 == dialog.m_list.GetColumnWidth(0, "health"));
 assert(CCSClientScoreBoardDialog::CSTRIKE_ACCOUNT_WIDTH == dialog.m_list.GetColumnWidth(0, "account"));
 assert(CCSClientScoreBoardDialog::CSTRIKE_ACCOUNT_WIDTH == dialog.m_list.rows[0].accountColumnWidthAtWrite);
 assert(std::string("$1600") == dialog.m_list.rows[0].data.GetString("account", ""));

 // Once a team player is alive with visible health, health opens on that same refresh
 g_PlayerExtraInfo[1].dead = false;
 dialog.UpdatePlayerInfo();
 assert(CCSClientScoreBoardDialog::CSTRIKE_HEALTH_WIDTH == dialog.m_list.GetColumnWidth(0, "health"));
 assert(CCSClientScoreBoardDialog::CSTRIKE_HEALTH_WIDTH == dialog.m_list.rows[0].healthColumnWidthAtWrite);
 assert(std::string("100") == dialog.m_list.rows[0].data.GetString("health", ""));
}

static void AvatarAndShortHeaderTogglesTakeEffectSameRefresh() {
 ResetTestWorld();
 g_CvarShowAvatars.value = 0.0f;
 g_CvarShortHeaders.value = 0.0f;
 CCSClientScoreBoardDialog dialog;
 dialog.Reset();

 SetPlayer(1, "Alice", 1, false, 100, 800, true);
 dialog.UpdatePlayerInfo();
 assert(1u == dialog.m_avatarEnabledDuringRowFill.size());
 assert(!dialog.m_avatarEnabledDuringRowFill[0]);
 assert(0 == dialog.m_list.GetColumnWidth(0, "avatar"));
 assert(!dialog.m_list.rows[0].data.HasKey("avatar"));

 // Enabling avatars and short headers together must apply to both columns and UpdatePlayerAvatar in the same refresh
 dialog.m_avatarEnabledDuringRowFill.clear();
 g_CvarShowAvatars.value = 1.0f;
 g_CvarShortHeaders.value = 1.0f;
 dialog.UpdatePlayerInfo();
 assert(1u == dialog.m_avatarEnabledDuringRowFill.size());
 assert(dialog.m_avatarEnabledDuringRowFill[0]);
 assert(CCSClientScoreBoardDialog::CSTRIKE_AVATAR_WIDTH == dialog.m_list.GetColumnWidth(0, "avatar"));
 assert(1 == dialog.m_list.rows[0].data.GetInt("avatar", 0));
 assert(std::wstring(L"HP") == dialog.m_list.GetColumnHeader(0, "health"));
 assert(CCSClientScoreBoardDialog::CSTRIKE_MIN_WIDTH == dialog.m_list.GetColumnWidth(0, "health"));
 assert(std::wstring(L"$") == dialog.m_list.GetColumnHeader(0, "account"));
 assert(CCSClientScoreBoardDialog::CSTRIKE_ACCOUNT_WIDTH == dialog.m_list.GetColumnWidth(0, "account"));
 assert(dialog.GetNameHeaderWidth() == dialog.m_list.GetColumnWidth(0, "name"));

 // Disabling avatars must stop UpdatePlayerAvatar from populating the 0-width avatar column in that same refresh
 dialog.m_avatarEnabledDuringRowFill.clear();
 g_CvarShowAvatars.value = 0.0f;
 dialog.UpdatePlayerInfo();
 assert(1u == dialog.m_avatarEnabledDuringRowFill.size());
 assert(!dialog.m_avatarEnabledDuringRowFill[0]);
 assert(0 == dialog.m_list.GetColumnWidth(0, "avatar"));
 assert(!dialog.m_list.rows[0].data.HasKey("avatar"));
}

static void CvarDisableHealthAndMoneySuppressesColumnsUntilReenabled() {
 ResetTestWorld();
 g_CvarShowHealth.value = 0.0f;
 g_CvarShowMoney.value = 0.0f;
 CCSClientScoreBoardDialog dialog;
 dialog.Reset();

 SetPlayer(1, "Alice", 1, false, 100, 800, true);
 dialog.UpdatePlayerInfo();
 assert(0 == dialog.m_list.GetColumnWidth(0, "health"));
 assert(0 == dialog.m_list.GetColumnWidth(0, "account"));
 assert(!dialog.m_list.rows[0].data.HasKey("health"));
 assert(!dialog.m_list.rows[0].data.HasKey("account"));
 assert(dialog.m_bHealthUpdateReceived);
 assert(dialog.m_bMoneyUpdateReceived);

 g_CvarShowHealth.value = 1.0f;
 g_CvarShowMoney.value = 1.0f;
 dialog.UpdatePlayerInfo();
 assert(CCSClientScoreBoardDialog::CSTRIKE_HEALTH_WIDTH == dialog.m_list.GetColumnWidth(0, "health"));
 assert(CCSClientScoreBoardDialog::CSTRIKE_ACCOUNT_WIDTH == dialog.m_list.GetColumnWidth(0, "account"));
 assert(std::string("100") == dialog.m_list.rows[0].data.GetString("health", ""));
 assert(std::string("$800") == dialog.m_list.rows[0].data.GetString("account", ""));
}

int main() {
 FirstRefreshOpensHealthAndMoneyColumns();
 DeadUnassignedOrNegativeValuesKeepColumnsCollapsed();
 AvatarAndShortHeaderTogglesTakeEffectSameRefresh();
 CvarDisableHealthAndMoneySuppressesColumnsUntilReenabled();
 return 0;
}
'''


def main():
    source = (ROOT / 'cstrike/cl_dll/VGUI/cstrikeclientscoreboard.cpp').read_text(encoding='utf-8')
    extracted = [
        PRELUDE,
        function(source, 'const wchar_t* CCSClientScoreBoardDialog::GetHealthHeader('),
        function(source, 'const wchar_t* CCSClientScoreBoardDialog::GetMoneyHeader('),
        function(source, 'const wchar_t* CCSClientScoreBoardDialog::GetScoreHeader('),
        function(source, 'const wchar_t* CCSClientScoreBoardDialog::GetDeathHeader('),
        function(source, 'const wchar_t* CCSClientScoreBoardDialog::GetPingHeader('),
        function(source, 'int CCSClientScoreBoardDialog::GetHeaderWidthNoScale('),
        function(source, 'int CCSClientScoreBoardDialog::GetHeaderWidth('),
        function(source, 'int CCSClientScoreBoardDialog::GetNameHeaderWidth('),
        function(source, 'int CCSClientScoreBoardDialog::GetAvatarHeightForScreenHeight('),
        function(source, 'int CCSClientScoreBoardDialog::GetAvatarHeaderWidth('),
        function(source, 'void CCSClientScoreBoardDialog::InitScoreboardSections('),
        function(source, 'void CCSClientScoreBoardDialog::AddHeader('),
        function(source, 'void CCSClientScoreBoardDialog::AddSection('),
        function(source, 'void CCSClientScoreBoardDialog::UpdateColumnInAllSections('),
        function(source, 'void CCSClientScoreBoardDialog::UpdateHeaders('),
        function(source, 'void CCSClientScoreBoardDialog::UpdateOptionalColumns('),
        function(source, 'static bool HasVisibleScoreboardHealth('),
        function(source, 'static bool HasVisibleScoreboardAccount('),
        function(source, 'bool CCSClientScoreBoardDialog::GetPlayerScoreInfo('),
        function(source, 'void CCSClientScoreBoardDialog::UpdatePlayerInfo('),
        TESTS,
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-cstrike-scoreboard-columns-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text('\n'.join(extracted), encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test')
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
