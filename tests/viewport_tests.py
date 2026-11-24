"""Exercise production viewport startup and message handlers without a renderer."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'

STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
#include "cl_dll/IVGuiClientDll.h"
#include "game_controls/viewport_message_reader.h"
#include "game_controls/viewport_player_info.h"
#include "const.h"
#include "Sequence.h"
#ifndef _WIN32
#include <strings.h>
#define _stricmp strcasecmp
#define _snwprintf swprintf
#endif
enum { MENU_MAPBRIEFING = 4, MENU_INTRO = 5 };
namespace vgui2 {
using VPANEL = unsigned int;
struct Localize {
 const wchar_t* Find(const char* token) { return !std::strcmp(token, "localized") ? L"Hello %s1, %s2" : L"No placeholders"; }
 void ConvertANSIToUnicode(const char* source, wchar_t* dest, int bytes) {
  int count = bytes / sizeof(wchar_t), i = 0;
  for (; source[i] && i < count-1; ++i) dest[i] = (unsigned char)source[i]; dest[i] = 0;
 }
} localizer;
Localize* localize() { return &localizer; }
}
struct Widget {
 static int live; bool visible = false;
 vgui2::VPANEL vpanel = 0, vparent = 0;
 Widget() { ++live; } virtual ~Widget() { --live; }
 void SetVisible(bool value) { visible = value; } bool IsVisible() { return visible; }
 void SetZPos(int) {}
 void SetParent(vgui2::VPANEL parent) { vparent = parent; }
 vgui2::VPANEL GetVPanel() const { return vpanel; }
 vgui2::VPANEL GetVParent() const { return vparent; }
 void SetMouseInputEnabled(bool) {}
};
int Widget::live = 0;
struct CClientScoreBoardDialog : Widget {
 std::string names[5]; int updates=0, rebuilds=0, resets=0, scoreTeam=0, frags=0, deaths=0;
 explicit CClientScoreBoardDialog(void*) {}
 void MakeReadyForUse() {} void Reset() { ++resets; }
 void SetTeamName(int team, const char* name) { assert(team>=0 && team<5); names[team]=name; }
 const char* GetTeamName(int team) { assert(team>=0 && team<5); return names[team].c_str(); }
 void SetTeamDetails(int team, int first, int second) { assert(team>=0 && team<5); scoreTeam=team; frags=first; deaths=second; }
 void Update(const char*, bool, bool) { ++updates; } void RebuildTeams(const char*, bool, bool) { ++rebuilds; }
};
struct CClientMOTD : Widget { explicit CClientMOTD(void*) {} };
struct CSpectatorGUI : Widget { explicit CSpectatorGUI(void*) {} void UpdateSpectatorPlayerList() {} };
struct CTeamMenu : Widget { explicit CTeamMenu(void*) {} void Update(const char*, bool, const char**, int) {} };
struct CClassMenu : Widget { CClassMenu(void*, bool) {} void Update(int*, int) {} };
struct CVGUITextWindow : Widget { explicit CVGUITextWindow(void*) {} };
struct TutorWindowManager : Widget { void CloseAllWindows() {} };
struct CCommandMenu : Widget { CCommandMenu(void*, const char*) {} void HideCommandMenu() {} };
struct TeamFortressViewport {
 struct CBackGroundPanel : Widget { explicit CBackGroundPanel(void*) {} };
 struct CSpecHelpWindow : Widget { CSpecHelpWindow(void*, CSpectatorGUI*) {} };
 static constexpr int TeamSlotCount=5, SpecialTeamCount=2;
 virtual ~TeamFortressViewport() {
  delete m_pBackGround; delete m_pClientScoreBoard; delete m_pMOTD; delete m_pSpectatorGUI;
  delete m_pTeamMenu; delete m_pClassMenu; delete m_pSpecHelp; delete m_pClassHelp;
  delete m_pMapBriefing; delete m_pTutorWindowManager; delete m_pCommandMenu;
  for (char* name : m_sTeamNames) std::free(name);
 }
 void Start(); void SetParent(vgui2::VPANEL parent); vgui2::VPANEL GetViewPortPanel(); virtual void HideAllVGUIMenu();
 virtual CBackGroundPanel* CreateBackgroundPanel() { return new CBackGroundPanel(nullptr); }
 virtual CClientScoreBoardDialog* CreateScoreBoard() { return new CClientScoreBoardDialog(nullptr); }
 virtual CClientMOTD* CreateMOTD() { return new CClientMOTD(nullptr); }
 virtual CSpectatorGUI* CreateSpectatorGUI() { return new CSpectatorGUI(nullptr); }
 virtual CTeamMenu* CreateTeamMenu() { return new CTeamMenu(nullptr); }
 virtual CClassMenu* CreateClassMenu() { return new CClassMenu(nullptr,true); }
 VGuiLibraryInterface_t callbacks = {};
 VGuiLibraryInterface_t* m_pClientDllInterface = &callbacks;
 VGuiLibraryInterface_t* GetClientDllInterface() { return m_pClientDllInterface; }
 bool SetTeamNames(const char* const*,int); void UpdateTeamMenu();
 void GetAllPlayersInfo() {} void CloseTutorTextWindow() {}
 int shown=0, lastMenu=0, tutorCalls=0, tutorId=0, tutorSpectator=0, tutorClass=0;
 std::wstring tutorText;
 void ShowVGUIMenu(int menu) { ++shown; lastMenu=menu; }
 void ShowTutorTextWindow(const wchar_t* text,int id,int messageClass,int spectator) { ++tutorCalls; tutorText=text; tutorId=id; tutorClass=messageClass; tutorSpectator=spectator; }
 void ShowTutorLine(int,int) {} void ShowTutorState(const wchar_t*) {}
 // MESSAGE_DECLARATIONS
 bool m_bInitialized=false, m_bIsFeigning=false;
 int m_iNumTeams=0, m_iCurrentTeamNumber=0, m_iGotAllMOTD=1, m_iRandomPC=0;
 int m_iAllowSpectators=0, m_iBuildState=0, m_iIsSettingDetpack=0;
 int m_iValidClasses[TeamSlotCount] = {};
 char* m_sTeamNames[TeamSlotCount] = {};
 char m_sMapName[64] = {}, m_szServerName[32] = {}, m_szMOTD[1536] = {};
 float m_flScoreBoardLastUpdated=0;
 CBackGroundPanel* m_pBackGround=nullptr;
 CClientScoreBoardDialog *m_pPrivateClientScoreBoard=nullptr, *m_pClientScoreBoard=nullptr;
 CSpectatorGUI *m_pPrivateSpectatorGUI=nullptr, *m_pSpectatorGUI=nullptr;
 CClientMOTD* m_pMOTD=nullptr; CTeamMenu* m_pTeamMenu=nullptr; CClassMenu* m_pClassMenu=nullptr;
 CSpecHelpWindow* m_pSpecHelp=nullptr; CVGUITextWindow *m_pClassHelp=nullptr, *m_pMapBriefing=nullptr;
 TutorWindowManager* m_pTutorWindowManager=nullptr; CCommandMenu* m_pCommandMenu=nullptr;
 struct { struct { void Purge() {} } m_heap; } m_PendingDialogs;
};
TeamFortressViewport* gViewPortInterface;
struct CustomViewport : TeamFortressViewport {
 int calls[6] = {};
 CBackGroundPanel* CreateBackgroundPanel() override { ++calls[0]; return new CBackGroundPanel(nullptr); }
 CClientScoreBoardDialog* CreateScoreBoard() override { ++calls[1]; return new CClientScoreBoardDialog(nullptr); }
 CClientMOTD* CreateMOTD() override { ++calls[2]; return new CClientMOTD(nullptr); }
 CSpectatorGUI* CreateSpectatorGUI() override { ++calls[3]; return new CSpectatorGUI(nullptr); }
 CTeamMenu* CreateTeamMenu() override { ++calls[4]; return new CTeamMenu(nullptr); }
 CClassMenu* CreateClassMenu() override { ++calls[5]; return nullptr; }
};
struct Player { char padding[60]; int teamnumber; char teamname[16]; int playerclass, deaths, frags; } players[4] = {};
int scoreWrites=0, teamWrites=0;
bool spectateOnly=false;
std::vector<unsigned char> TextMessage(int first, const std::string& text) {
 std::vector<unsigned char> result={static_cast<unsigned char>(first)};
 result.insert(result.end(),text.begin(),text.end()); result.push_back(0); return result;
}
void AppendString(std::vector<unsigned char>& data,const std::string& text) { data.insert(data.end(),text.begin(),text.end()); data.push_back(0); }
'''

TESTS = r'''
int main() {
 {
  CustomViewport view; gViewPortInterface=&view;
  view.callbacks.GetMaxPlayers=[](){return 3;}; view.callbacks.TeamPlay=[](){return 1;};
  view.callbacks.IsSpectateOnly=[](){return spectateOnly;};
  view.callbacks.SetPlayerScoreInfo=[](int player,int frags,int deaths,int playerClass,int team){ ++scoreWrites; UpdateViewportPlayerScore(players,player,frags,deaths,playerClass,team); };
  view.callbacks.SetPlayerTeamInfo=[](int player,const char* team){ ++teamWrites; UpdateViewportPlayerTeam(players,player,team); };
  assert(0==view.GetViewPortPanel());
  view.Start(); for (int calls : view.calls) assert(calls==1);
  view.m_pBackGround->vpanel=101;
  view.SetParent(42);
  assert(42==view.GetViewPortPanel());
  int live=Widget::live; view.Start(); assert(live==Widget::live); for (int calls : view.calls) assert(calls==1);
  assert(!view.m_pClassMenu && view.m_pClientScoreBoard->resets==1);
  unsigned char score[]={2,42,0,0xfe,0xff,5,0,0xff,0xff};
  for (int length=0;length<int(sizeof(score));++length) view.MsgFunc_ScoreInfo("",length,score);
  assert(scoreWrites==0 && view.m_pClientScoreBoard->updates==0);
  view.MsgFunc_ScoreInfo("",sizeof(score),score);
  assert(scoreWrites==1 && players[2].frags==42 && players[2].deaths==-2 && players[2].playerclass==5 && players[2].teamnumber==0);
  assert(players[1].frags==0 && players[3].frags==0);
  for (unsigned char invalid : {0,4,255}) { score[0]=invalid; view.MsgFunc_ScoreInfo("",sizeof(score),score); } assert(scoreWrites==1);
  auto team=TextMessage(2,std::string(300,'t'));
  view.MsgFunc_TeamInfo("",int(team.size()-1),team.data()); assert(teamWrites==0);
  view.MsgFunc_TeamInfo("",int(team.size()),team.data()); assert(teamWrites==1 && std::strlen(players[2].teamname)==sizeof(players[2].teamname)-1);
  assert(!players[1].teamname[0] && !players[3].teamname[0]);
  std::vector<unsigned char> names={3}; for (const char* name : {"one","two","three"}) AppendString(names,name);
  for (int length=0;length<int(names.size());++length) view.MsgFunc_TeamNames("",length,names.data()); assert(view.m_iNumTeams==0);
  view.MsgFunc_TeamNames("",int(names.size()),names.data()); assert(view.m_iNumTeams==3 && !std::strcmp("one",view.m_sTeamNames[0]));
  assert(!std::strcmp("#Team_AutoAssign",view.m_sTeamNames[3]) && !std::strcmp("#Menu_Spectate",view.m_sTeamNames[4]));
  for (unsigned char invalid : {4,255}) { names[0]=invalid; view.MsgFunc_TeamNames("",int(names.size()),names.data()); } assert(view.m_iNumTeams==3);
  auto motd=TextMessage(0,std::string(5000,'m')); view.MsgFunc_MOTD("",int(motd.size()),motd.data()); assert(std::strlen(view.m_szMOTD)==sizeof(view.m_szMOTD)-1);
  motd=TextMessage(1,"end"); view.MsgFunc_MOTD("",int(motd.size()),motd.data()); assert(view.shown==1 && view.lastMenu==MENU_INTRO);
  const std::string saved=view.m_szMOTD; view.MsgFunc_MOTD("",int(motd.size()-1),motd.data()); assert(saved==view.m_szMOTD);
  spectateOnly=true; motd=TextMessage(1,"observer"); view.MsgFunc_MOTD("",int(motd.size()),motd.data()); assert(view.shown==1 && !std::strcmp("observer",view.m_szMOTD));
  std::vector<unsigned char> tutor; AppendString(tutor,"no-placeholders"); tutor.push_back(2); AppendString(tutor,"unused"); AppendString(tutor,"also unused");
  tutor.insert(tutor.end(),{123,0,1,0,4,0});
  for (int length=0;length<int(tutor.size());++length) view.MsgFunc_TutorText("",length,tutor.data()); assert(view.tutorCalls==0);
  view.MsgFunc_TutorText("",int(tutor.size()),tutor.data()); assert(view.tutorCalls==1 && view.tutorId==123 && view.tutorSpectator==1 && view.tutorClass==4);
  tutor.clear(); AppendString(tutor,"localized"); tutor.push_back(2); AppendString(tutor,std::string(5000,'x')); AppendString(tutor,"suffix"); tutor.insert(tutor.end(),{1,0,0,0,2,0});
  view.MsgFunc_TutorText("",int(tutor.size()),tutor.data()); assert(view.tutorCalls==2 && view.tutorText.size()<=1023);
  unsigned char classes[]={1,0,2,0,3,0,4,0,5,0}; view.MsgFunc_ValClass("",9,classes); assert(view.m_iValidClasses[0]==0);
  view.MsgFunc_ValClass("",sizeof(classes),classes); assert(view.m_iValidClasses[0]==1 && view.m_iValidClasses[4]==5);
 }
 assert(Widget::live==0);
}
'''


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def main():
    viewport = (ROOT / 'vgui2/game_controls/vgui_TeamFortressViewport.cpp').read_text(encoding='utf-8')
    messages = (ROOT / 'vgui2/game_controls/vgui_TeamFortressViewport_msgs.cpp').read_text(encoding='utf-8')
    declarations = '\n'.join(' int ' + name + '(const char*,int,void*);' for name in re.findall(r'int TeamFortressViewport::(MsgFunc_\w+)\(', messages))
    code = STUBS.replace('// MESSAGE_DECLARATIONS', declarations)
    for name in ('Start', 'SetParent', 'HideAllVGUIMenu'):
        code += function(viewport, 'void TeamFortressViewport::' + name + '(')
    code += function(viewport, 'vgui2::VPANEL TeamFortressViewport::GetViewPortPanel(')
    code += re.sub(r'^#include[^\n]*\n', '', messages, flags=re.M) + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-viewport-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        executable = compile_program([path / 'test.cpp'], path / 'test', includes=[ROOT / 'public', ROOT / 'engine', ROOT / 'common'],
                                     defines=['_CRT_SECURE_NO_WARNINGS'])
        subprocess.run([str(executable)], check=True, timeout=20)


if __name__ == '__main__':
    main()
