"""Exercise production browser connection commands and engine signon with stubs.

Run: python tests/spectate_connection_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def function(source, signature):
    start = re.search(re.escape(signature) + r'[^\n;]*\n\{', source).start()
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
struct gameserveritem_t {
 struct { const char* GetConnectionAddressString() const { return "127.0.0.1:27015"; } } m_NetAdr;
};
struct Engine {
 std::vector<std::string> commands;
 void AddTextCommand(const char* text) { commands.emplace_back(text); }
} engine, *g_pRunGameEngine = &engine;
struct CDialogGameInfo {
 CDialogGameInfo() = default;
 CDialogGameInfo(void*, int, int, unsigned short, bool hltvOnly=false) : m_bHLTVOnly(hltvOnly) {}
 bool m_bHLTVOnly = false;
 char m_szPassword[64] = {};
 void SetParent(int) {} void AddActionSignalTarget(void*) {} void Run(const char*) {}
 void Connect();
 void ApplyConnectCommand(const gameserveritem_t&);
 void ConstructConnectArgs(char*, int, const gameserveritem_t&);
};
'''

TESTS = r'''
static void BrowserCommands() {
 gameserveritem_t server;
 CDialogGameInfo dialog;
 dialog.m_bHLTVOnly = true;
 dialog.ApplyConnectCommand(server);
 assert(engine.commands.back() == "connect_hltv 127.0.0.1:27015\n");
 char options[256]; dialog.ConstructConnectArgs(options, sizeof(options), server);
 assert(std::string(options) == " +connect_hltv 127.0.0.1:27015");
 dialog.m_bHLTVOnly = false;
 dialog.ApplyConnectCommand(server);
 assert(engine.commands.back() == "connect 127.0.0.1:27015\n");
 strcpy(dialog.m_szPassword, "test-password"); dialog.m_bHLTVOnly=true;
 dialog.ApplyConnectCommand(server);
 assert(engine.commands[engine.commands.size()-2] == "password \"test-password\"\n");
 assert(engine.commands.back() == "connect_hltv 127.0.0.1:27015\n");
 puts("PASS: browser and launch commands retain HLTV intent with passwords");
}
'''

BROWSER_STUBS = r'''
struct serverdisplay_t {
 struct { int GetIP(){return 0x7f000001;} int GetQueryPort(){return 27015;} unsigned short GetConnectionPort(){return 27015;} } m_netAdr;
};
struct IGameList {
 serverdisplay_t server;
 serverdisplay_t* GetServer(unsigned int){return &server;}
};
struct Selection {
 int selected=1;
 int GetSelectedItemsCount(){return selected;} int GetSelectedItem(int){return 0;}
 unsigned int GetItemUserData(int){return 0;}
};
constexpr int eInternetServer=0,eSpectatorServer=1;
struct CBaseGamesPage : IGameList {
 Selection selection, *m_pGameList=&selection;
 int m_eMatchMakingType=eInternetServer;
 void StopRefresh(){} void OnBeginConnect(); void OnViewGameInfo();
};
struct Dialogs {
 std::vector<CDialogGameInfo*> items;
 int AddToTail(){items.push_back(nullptr);return int(items.size())-1;}
 CDialogGameInfo*& operator[](int i){return items[i];}
 ~Dialogs(){for(auto* item:items)delete item;}
};
struct CServerBrowserDialog {
 Dialogs m_GameInfoDialogs;
 int GetVParent(){return 0;}
 CDialogGameInfo* JoinGame(IGameList*,unsigned int,bool=false);
 CDialogGameInfo* OpenGameInfoDialog(IGameList*,unsigned int,bool=false);
} browser;
CServerBrowserDialog& ServerBrowserDialog(){return browser;}
'''

ENGINE_STUBS = r'''
#include <cstdlib>
#include <cstdint>
#include <cstdarg>
#include <map>
#if defined(_WIN32)
#define Q_strcasecmp _stricmp
#define Q_strnicmp _strnicmp
#else
#include <strings.h>
#define Q_strcasecmp strcasecmp
#define Q_strnicmp strncasecmp
#endif
using byte = unsigned char; using qboolean = int; using CareerStateType = int;
constexpr int TRUE=1, FALSE=0, MAX_PATH=260, CAREER_NONE=0, CAREER_LOADING=1;
constexpr int ca_disconnected=0, ca_connecting=1, ca_connected=2, ca_active=3, ca_uninitialized=4;
constexpr int NA_UNUSED=0, NA_LOOPBACK=1, NS_CLIENT=0, PORT_SERVER=27015, CL_CONNECTION_RETRIES=5;
constexpr int SIGNONS=4, clc_stringcmd=3, PROTOCOL_STEAM=3, PROTOCOL_HASHEDCDKEY=2, PROTOCOL_VERSION=48;
constexpr int HLTV_ACTIVE=0, HLTV_STATUS=1, HLTV_LISTEN=2;
#define Q_strncpy std::strncpy
#define Q_strcpy std::strcpy
#define Q_memset std::memset
#define Q_strstr std::strstr
#define Q_strlen std::strlen
#define Q_atoi std::atoi
struct netadr_t { int type=0; unsigned short port=0; } g_GameServerAddress, net_from;
struct Channel { int sock=0, message=0; netadr_t remote_address; };
struct {
 double connect_time=0; int connect_retry=0,state=ca_disconnected,signon=0;
 bool demoplayback=false,demorecording=false,passive=false,spectator=false,isVAC2Secure=false,timedemo=false;
 int GameServerSteamID=0,build_num=0,challenge=0,authprotocol=0;
 char servername[260]={},userinfo[1024]={}; Channel netchan; netadr_t connect_stream,game_stream;
} cls;
struct { const char* levelname="maps/crossfire.bsp"; } cl;
struct Cvar { float value=0; } scr_downloading,sys_timescale,sv_allow_autoaim,sv_allow_shaders,cl_resend{2},fs_startup_timings;
int g_bRedirectedToProxy=0;
static qboolean g_bConnectingToHLTV, g_bRetryHLTV;
int g_careerState=0,src_command=0,cmd_source=0,num_servers=0,msg_buckets[1],total_data[1];
bool g_bIsCStrike=false,g_bIsCZero=false,gfExtendedError=false,serverActive=false,addressValid=true;
bool senderValid=true;
float g_LastScreenUpdateTime=0; double realtime=100;
struct { netadr_t adr; } cached_servers[1];
std::string args="127.0.0.1:27015", queued;
int argc=2, disconnectNotifications=0, packets=0, commandsSent=0;
constexpr int CMD_FLAG_PRIVILEGED=1;
std::map<std::string, void(*)()> registeredCommands;
void Cmd_AddCommandWithFlags(const char* name, void(*callback)(), int flags) {
 assert(flags==CMD_FLAG_PRIVILEGED); registeredCommands[name]=callback;
}
int Cmd_Argc(){return argc;} const char* Cmd_Args(){return args.c_str();}
void SetCStrikeFlags(){} void Con_Printf(const char*,...){} void Con_DPrintf(const char*,...){}
void CL_StopPlayback(){cls.demoplayback=false;} void SetCareerAudioState(int){} void SPR_Shutdown_NoModelFree(){}
void S_StopAllSounds(int){} void CL_Stop_f(){} void FS_LogLevelLoadStarted(const char*){}
void NET_LeaveGroup(int,netadr_t){} void Netchan_Transmit(Channel*,int,const void*){}
void Steam_GSTerminateGameConnection(){} bool Host_IsServerActive(){return serverActive;}
void Host_ShutdownServer(int){serverActive=false;} void CloseSecurityModule(){} void CL_ClearState(int){}
void NET_CheckCleanupFakeIPConnection(int,netadr_t*){} void Netchan_Clear(Channel*){}
void CL_DeallocateDynamicData(){} void Cvar_DirectSet(Cvar*,const char*){}
void VGuiWrap2_NotifyOfServerDisconnect(){++disconnectNotifications;} void StopLoadingProgressBar(){}
void StartLoadingProgressBar(const char*,int){} void SetLoadingProgressBarStatusText(const char*){}
const char* NET_AdrToString(netadr_t){return "127.0.0.1:27015";} void NET_Config(int){}
void Cache_Report(){} void MSG_WriteByte(int*,int){} void MSG_WriteString(int*,const char*){++commandsSent;}
void SCR_EndLoadingPlaque(){} void FS_LogLevelLoadFinished(const char*){}
void VGuiWrap2_LoadingFinished(const char*,const char*){} void Sequence_OnLevelLoad(const char*){}
void COM_ExplainDisconnection(int,const char*,...){} void Cvar_SetValue(const char*,float){}
bool NET_StringToAdr(const char*,netadr_t* adr){adr->port=27015;return addressValid;}
short BigShort(int n){return n;} void NET_SendPacket(int,int,const void*,netadr_t){++packets;}
void ContinueLoadingProgressBar(const char*,int,float){} void SetupStartupTimings(){}
const char* CL_GetCDKeyHash(){return "hash";} const char* va(const char*,...){return "3";}
void Info_SetValueForStarKey(char*,const char*,const char*,int){}
unsigned int NET_NetAdr_GetIPNetworkByteOrder(netadr_t*){return 0;}
unsigned int htonl(unsigned int n){return n;}
int Steam_GSInitiateGameConnection(char*,int,int,unsigned int,int,bool){return 0;}
void CL_Disconnect_f(); void CL_SendConnectPacket(); void CL_Retry_f_0();
bool IsFromConnectedServer(netadr_t){return senderValid;} void Cbuf_AddText(const char* text){queued+=text;}
int MSG_ReadByte(){return HLTV_ACTIVE;} void MSG_ReadLong(){} void MSG_ReadShort(){} void MSG_ReadWord(){} void CL_ParseListen(){}
'''

ENGINE_TESTS = r'''
void CL_Disconnect_f(){CL_Disconnect();}
static void Execute(const std::string& command) {
 auto space=command.find(' '); args=command.substr(space+1);
 if (!args.empty() && args.back()=='\n') args.pop_back();
 auto entry=registeredCommands.find(command.substr(0,space));
 assert(entry!=registeredCommands.end()); entry->second();
}
void CDialogGameInfo::Connect() {
 gameserveritem_t server; ApplyConnectCommand(server); Execute(engine.commands.back());
}
static void Finish(bool spectator) {
 cls.signon=SIGNONS; cls.spectator=spectator; CL_SignonReply();
}
static void BrowserToEngine() {
 for(int type : {eSpectatorServer,eInternetServer}) {
  for(bool info : {false,true}) {
   CBaseGamesPage page; page.m_eMatchMakingType=type;
   if(info) {
    page.OnViewGameInfo(); browser.m_GameInfoDialogs.items.back()->Connect();
   } else page.OnBeginConnect();
   assert(bool(g_bConnectingToHLTV)==(type==eSpectatorServer));
   int before=disconnectNotifications; Finish(false);
   assert(disconnectNotifications==before+(type==eSpectatorServer?1:0));
  }
 }
 size_t before=browser.m_GameInfoDialogs.items.size();
 CBaseGamesPage page; page.selection.selected=0; page.OnBeginConnect(); page.OnViewGameInfo();
 assert(browser.m_GameInfoDialogs.items.size()==before);
 puts("PASS: actual page handlers and browser join/details paths reach the correct engine entry");
}
static void EngineConnections() {
 Execute("connect_hltv 127.0.0.1:27015");
 assert(cls.state==ca_connecting && g_bConnectingToHLTV);
 int notifications=disconnectNotifications; Finish(false);
 assert(disconnectNotifications==notifications+1 && cls.state==ca_disconnected);
 assert(!g_bConnectingToHLTV && !g_bRedirectedToProxy);
 Execute("connect_hltv 127.0.0.1:27015"); CL_ParseHLTV();
 assert(cls.spectator && g_bConnectingToHLTV);
 notifications=disconnectNotifications; Finish(true); assert(disconnectNotifications==notifications);
 assert(!g_bConnectingToHLTV);
 Execute("connect 127.0.0.1:27015"); notifications=disconnectNotifications;
 Finish(false); assert(disconnectNotifications==notifications);
 puts("PASS: Spectate rejects game servers, allows HLTV, ordinary connections remain allowed");

 for(int signon : {1,2,3}) {
  Execute("connect_hltv 127.0.0.1:27015"); cls.signon=signon;
  notifications=disconnectNotifications; CL_SignonReply();
  assert(disconnectNotifications==notifications && g_bConnectingToHLTV);
  Finish(false); assert(disconnectNotifications==notifications+1);
 }
 puts("PASS: validation runs only at final signon");

 for(bool spectator : {false,true}) {
  Execute("connect_hltv 127.0.0.1:27015");
  ReceiveRedirect("L127.0.0.2:27015"); assert(!queued.empty() && g_bRedirectedToProxy);
  Execute(queued); queued.clear(); assert(g_bConnectingToHLTV && g_bRedirectedToProxy);
  ReceiveRedirect("L127.0.0.3:27015"); Execute(queued); queued.clear();
  assert(g_bConnectingToHLTV && g_bRedirectedToProxy);
  notifications=disconnectNotifications; if(spectator)CL_ParseHLTV(); Finish(spectator);
  assert(disconnectNotifications==notifications+(spectator?0:1));
 }
 // The legacy redirect restriction applies even to ordinary connection attempts.
 Execute("connect 127.0.0.1:27015"); ReceiveRedirect("L127.0.0.2:27015");
 Execute(queued); queued.clear(); notifications=disconnectNotifications; Finish(false);
 assert(disconnectNotifications==notifications+1);
 // Also retain the original flag-only check independently of the new intent.
 Execute("connect 127.0.0.1:27015"); g_bRedirectedToProxy=true;
 notifications=disconnectNotifications; Finish(false); assert(disconnectNotifications==notifications+1);
 puts("PASS: single/multiple redirects retain HLTV requirement and legacy restriction");

 Execute("connect_hltv 127.0.0.1:27015"); senderValid=false;
 ReceiveRedirect("L127.0.0.2:27015"); assert(queued.empty()); senderValid=true;
 ReceiveRedirect("Lbad;quit"); assert(queued.empty() && !g_bConnectingToHLTV);
 puts("PASS: redirect sender and command-separator validation is preserved");

 for(int failure=0;failure<6;++failure) {
  Execute("connect_hltv 127.0.0.1:27015");
  g_bRedirectedToProxy=true;
  if(failure==0) CL_Disconnect_f();
  if(failure==1) CL_Disconnect();
  if(failure==2) {addressValid=false;CL_CheckForResend();addressValid=true;}
  if(failure==3) {cls.connect_retry=CL_CONNECTION_RETRIES;CL_CheckForResend();}
  if(failure==4) {cls.passive=true;cls.challenge=0;CL_CheckForResend();}
  if(failure==5) {addressValid=false;CL_SendConnectPacket();addressValid=true;}
  assert(!g_bConnectingToHLTV && !g_bRedirectedToProxy);
  Execute("connect 127.0.0.1:27015"); notifications=disconnectNotifications;
  Finish(false); assert(disconnectNotifications==notifications);
 }
 Execute("connect_hltv 127.0.0.1:27015"); Execute("connect 127.0.0.2:27015");
 assert(!g_bConnectingToHLTV); notifications=disconnectNotifications; Finish(false);
 assert(disconnectNotifications==notifications);
 puts("PASS: cancel, disconnect, DNS failure, retry exhaustion, multicast failure and replacement clear intent");

 Execute("connect_hltv 127.0.0.1:27015"); argc=1; CL_Connect_f(); argc=2;
 assert(g_bConnectingToHLTV);
 g_bIsCStrike=true; cmd_source=src_command+1; CL_Connect_f();
 assert(g_bConnectingToHLTV); g_bIsCStrike=false; cmd_source=src_command;
 CL_CheckForResend(); assert(g_bConnectingToHLTV && packets>0);
 cls.passive=true; cls.challenge=1; cls.connect_time=0; CL_CheckForResend();
 assert(!cls.passive && g_bConnectingToHLTV);
 Finish(false);
 cls.state=ca_disconnected; g_bConnectingToHLTV=true; serverActive=true;
 CL_CheckForResend(); assert(!g_bConnectingToHLTV && cls.state==ca_connecting);
 serverActive=false; CL_Disconnect();
 puts("PASS: ignored commands and connection retries preserve intent; a new local connection clears it");
}
static void RetryCommand(const char* expected) {
 Execute("retry"); assert(queued==expected); Execute(queued); queued.clear();
}
static void RetryConnections() {
 // HTTP downloads disconnect before signon and reconnect through retry.
 Execute("connect_hltv 127.0.0.1:27015"); CL_Disconnect(); assert(!g_bConnectingToHLTV);
 RetryCommand("connect_hltv 127.0.0.1:27015\n"); assert(g_bConnectingToHLTV);
 int notifications=disconnectNotifications; Finish(false);
 assert(disconnectNotifications==notifications+1);
 // A later map download must not turn an accepted HLTV session into a game connection.
 Execute("connect_hltv 127.0.0.1:27015"); CL_ParseHLTV(); Finish(true);
 CL_Disconnect(); RetryCommand("connect_hltv 127.0.0.1:27015\n");
 assert(!cls.spectator && g_bConnectingToHLTV);
 notifications=disconnectNotifications; Finish(false);
 assert(disconnectNotifications==notifications+1);
 Execute("connect_hltv 127.0.0.1:27015"); CL_ParseHLTV(); CL_Disconnect();
 RetryCommand("connect_hltv 127.0.0.1:27015\n"); CL_ParseHLTV();
 notifications=disconnectNotifications; Finish(true); assert(disconnectNotifications==notifications);
 // Ordinary, local and passive connections keep their original retry commands.
 Execute("connect 127.0.0.1:27015"); CL_Disconnect(); RetryCommand("connect 127.0.0.1:27015\n");
 assert(!g_bConnectingToHLTV); notifications=disconnectNotifications; Finish(false);
 assert(disconnectNotifications==notifications);
 Execute("connect_hltv 127.0.0.1:27015"); CL_Disconnect();
 serverActive=true; CL_CheckForResend(); serverActive=false; CL_Disconnect();
 Execute("retry"); assert(queued=="connect localhost\n"); queued.clear();
 cls.passive=true; Execute("retry"); assert(queued=="listen localhost\n"); queued.clear(); cls.passive=false;
 puts("PASS: retry after download disconnects keeps HLTV intent; other retries are unchanged");
}
'''


def main():
    source = (ROOT / 'serverbrowser/DialogGameInfo.cpp').read_text()
    code = STUBS + function(source, 'void CDialogGameInfo::ApplyConnectCommand(')
    code += function(source, 'void CDialogGameInfo::ConstructConnectArgs(') + TESTS
    code += BROWSER_STUBS
    source = (ROOT / 'serverbrowser/ServerBrowserDialog.cpp').read_text()
    code += function(source, 'CDialogGameInfo *CServerBrowserDialog::JoinGame( IGameList')
    code += function(source, 'CDialogGameInfo *CServerBrowserDialog::OpenGameInfoDialog( IGameList')
    source = (ROOT / 'serverbrowser/BaseGamesPage.cpp').read_text()
    code += function(source, 'void CBaseGamesPage::OnBeginConnect()')
    code += function(source, 'void CBaseGamesPage::OnViewGameInfo()')
    source = (ROOT / 'engine/cl_main.cpp').read_text()
    code += ENGINE_STUBS
    for signature in ['static void CL_ClearConnectionIntent()', 'void CL_Disconnect()',
                      'static void CL_Connect(', 'void CL_Connect_f()',
                      'static void CL_ConnectHLTV_f()', 'void CL_SendConnectPacket()',
                      'void CL_Retry_f()', 'void CL_Retry_f_0()',
                      'void CL_CheckForResend()', 'void CL_SignonReply()']:
        code += function(source, signature)
    registrations = re.findall(r'\tCmd_AddCommandWithFlags\( "(?:connect|connect_hltv|retry)",[^\n]+', source)
    code += '\nvoid RegisterConnectionCommands() {\n' + '\n'.join(registrations) + '\n}\n'
    code += function((ROOT / 'engine/cl_parse.cpp').read_text(), 'void CL_ParseHLTV()')
    # Compile the real packet case, retaining its early-break validation paths.
    start = source.index('\tcase S2C_REDIRECT:')
    redirect = source[start:source.index('\n\tcase S2C_CONNREJECT:', start)]
    code += '\nvoid ReceiveRedirect(const char* s) { const char* server; char redirect[1024]; switch(0) {\n'
    code += redirect.replace('case S2C_REDIRECT:', 'case 0:') + '\n}}\n'
    code += ENGINE_TESTS
    code += '\nint main() { RegisterConnectionCommands(); assert(registeredCommands.size()==3); BrowserCommands(); BrowserToEngine(); EngineConnections(); RetryConnections(); }\n'
    with tempfile.TemporaryDirectory(prefix='goldsrc-spectate-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        executable = compile_program([path / 'test.cpp'], path / 'test')
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
