"""Exercise the production demo events list refresh with a copying list panel and counted KeyValues.

Run: python tests/gameui_demo_events_tests.py
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
static int liveKeyValues = 0;
class KeyValues {
public:
 KeyValues(const char* name, const char* firstKey, const char* firstValue, const char* secondKey, const char* secondValue)
  : m_name(name), m_keys{firstKey, secondKey}, m_values{firstValue, secondValue} { ++liveKeyValues; }
 KeyValues(const KeyValues& other) : m_name(other.m_name), m_keys(other.m_keys), m_values(other.m_values) { ++liveKeyValues; }
 ~KeyValues() { --liveKeyValues; }
 KeyValues* MakeCopy() const { return new KeyValues(*this); }
 void deleteThis() { delete this; }
 const char* GetString(const char* key) const {
  for (size_t i = 0; i < m_keys.size(); ++i)
   if (m_keys[i] == key) return m_values[i].c_str();
  return "";
 }
 std::string m_name;
private:
 std::vector<std::string> m_keys, m_values;
};
namespace vgui2 {
// keeps its own copy of each item like ListPanel::AddItem and frees the copies in DeleteAllItems
class ListPanel {
public:
 ~ListPanel() { DeleteAllItems(); }
 int AddItem(const KeyValues* data, unsigned int, bool, bool) { m_items.push_back(data->MakeCopy()); return (int)m_items.size() - 1; }
 void DeleteAllItems() { for (KeyValues* item : m_items) item->deleteThis(); m_items.clear(); m_selected = -1; }
 int GetItemIDFromRow(int row) { return row >= 0 && row < (int)m_items.size() ? row : -1; }
 void SetSingleSelectedItem(int itemID) { m_selected = itemID; }
 std::vector<KeyValues*> m_items;
 int m_selected = -1;
};
}
class DirectorCmd {
public:
 DirectorCmd(int index, float time, const char* text) : m_Time(time), m_Index(index), m_Text(text) {}
 char* ToString() { return &m_Text[0]; }
 float m_Time;
 int m_Index;
private:
 std::string m_Text;
};
class IObjectContainer {
public:
 void* GetFirst() { m_next = 0; return GetNext(); }
 void* GetNext() { return m_next < m_commands.size() ? m_commands[m_next++] : nullptr; }
 std::vector<DirectorCmd*> m_commands;
private:
 size_t m_next = 0;
};
class IDemoPlayer {
public:
 IObjectContainer* GetCommands() { return m_hasCommands ? &m_commands : nullptr; }
 DirectorCmd* GetLastCommand() { return m_last; }
 IObjectContainer m_commands;
 DirectorCmd* m_last = nullptr;
 bool m_hasCommands = true;
};
// only the members OnUpdate uses
class CDemoEventsDialog {
public:
 explicit CDemoEventsDialog(IDemoPlayer* demoPlayer) : m_EventList(new vgui2::ListPanel), m_DemoPlayer(demoPlayer) {}
 ~CDemoEventsDialog() { delete m_EventList; }
 void OnUpdate();
 vgui2::ListPanel* m_EventList;
 IDemoPlayer* m_DemoPlayer;
 DirectorCmd* m_CurrentCmd = nullptr;
};
'''

TESTS = r'''
static void RefreshKeepsOnlyTheListCopies() {
 DirectorCmd first(1, 61.25f, "Camera"), second(2, 125.5f, "Event");
 IDemoPlayer player;
 player.m_commands.m_commands = {&first, &second};
 player.m_last = &second;
 {
  CDemoEventsDialog dialog(&player);
  for (int refresh = 0; refresh < 3; ++refresh) {
   dialog.OnUpdate();
   assert(2 == liveKeyValues && 2 == dialog.m_EventList->m_items.size());
  }
  KeyValues* item = dialog.m_EventList->m_items[1];
  assert(std::string("2") == item->m_name);
  assert(0 == strcmp("002:05:50", item->GetString("time")) && 0 == strcmp("Event", item->GetString("type")));
  assert(1 == dialog.m_EventList->m_selected && &second == dialog.m_CurrentCmd);
 }
 assert(0 == liveKeyValues);
 puts("PASS: refreshing the event list frees each source KeyValues and keeps only the list copies");
}

static void RefreshFollowsAddedAndRemovedEvents() {
 DirectorCmd first(1, 1.0f, "Camera"), second(2, 2.0f, "Event"), third(3, 3.0f, "Fade");
 IDemoPlayer player;
 player.m_commands.m_commands = {&first, &second};
 {
  CDemoEventsDialog dialog(&player);
  dialog.OnUpdate();
  player.m_commands.m_commands.push_back(&third);
  dialog.OnUpdate();
  assert(3 == liveKeyValues && 0 == dialog.m_EventList->m_selected);
  player.m_commands.m_commands = {&third};
  dialog.OnUpdate();
  assert(1 == liveKeyValues);
  player.m_hasCommands = false;
  dialog.OnUpdate();
  assert(0 == liveKeyValues);
 }
 assert(0 == liveKeyValues);
 puts("PASS: refreshing after events are added or removed keeps one KeyValues per listed event");
}
'''


def main():
    source = (ROOT / 'GameUI/DemoEventsDialog.cpp').read_text(encoding='utf-8')
    code = STUBS + function(source, 'void CDemoEventsDialog::OnUpdate(') + TESTS
    code += '\nint main() { RefreshKeepsOnlyTheListCopies(); RefreshFollowsAddedAndRemovedEvents(); }\n'
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-demo-events-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
