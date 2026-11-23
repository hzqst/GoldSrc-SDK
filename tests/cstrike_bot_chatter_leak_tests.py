"""Regression tests for BotChatterInterface::AddStatement deleting rejected statements (#508)."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

SDK_ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def extract(relpath, signature):
    source = (SDK_ROOT / relpath).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cctype>

using std::toupper;

struct cvar_t {
    const char* string = "normal";
};
static cvar_t cv_bot_chatter;

static int g_liveStatements = 0;

struct BotStatement {
    BotStatement* m_next = nullptr;
    BotStatement* m_prev = nullptr;
    int m_phraseCount = 1;
    float m_startTime = 0.0f;
    bool important = false;
    bool redundant = false;

    BotStatement(float startTime = 0.0f, bool isImportant = false, int phraseCount = 1, bool isRedundant = false)
        : m_phraseCount(phraseCount), m_startTime(startTime), important(isImportant), redundant(isRedundant) {
        ++g_liveStatements;
    }
    ~BotStatement() {
        --g_liveStatements;
    }

    bool IsImportant() const { return important; }
    bool IsRedundant(const BotStatement*) const { return redundant; }
};

struct CCSBot {
    bool alive = true;
    bool IsAlive() const { return alive; }
    void PrintIfWatched(const char*) {}
};

struct BotChatterInterface {
    CCSBot* m_bot = nullptr;
    BotStatement* m_statementList = nullptr;

    ~BotChatterInterface() {
        ClearAll();
    }

    void ClearAll() {
        BotStatement* cur = m_statementList;
        while (cur) {
            BotStatement* next = cur->m_next;
            delete cur;
            cur = next;
        }
        m_statementList = nullptr;
    }

    void AddStatement( BotStatement *statement, bool mustSpeak = false );
};
'''

TESTS = r'''
int main() {
    CCSBot bot;
    BotChatterInterface chatter;
    chatter.m_bot = &bot;

    // 1. NULL statement is ignored safely.
    cv_bot_chatter.string = "normal";
    bot.alive = true;
    chatter.AddStatement(nullptr, false);
    assert(0 == g_liveStatements);
    assert(nullptr == chatter.m_statementList);

    // 2. bot_chatter "off" deletes both normal and important statements.
    cv_bot_chatter.string = "off";
    chatter.AddStatement(new BotStatement(1.0f, false), false);
    chatter.AddStatement(new BotStatement(2.0f, true), true);
    assert(0 == g_liveStatements);
    assert(nullptr == chatter.m_statementList);

    // 3. bot_chatter "minimal" deletes unimportant statements and keeps important ones.
    cv_bot_chatter.string = "minimal";
    chatter.AddStatement(new BotStatement(1.0f, false), false);
    assert(0 == g_liveStatements);
    assert(nullptr == chatter.m_statementList);

    BotStatement* importantStmt = new BotStatement(2.0f, true);
    chatter.AddStatement(importantStmt, false);
    assert(1 == g_liveStatements);
    assert(importantStmt == chatter.m_statementList);
    chatter.ClearAll();
    assert(0 == g_liveStatements);

    // 4. Dead bot with mustSpeak == false deletes the statement; mustSpeak == true keeps it.
    cv_bot_chatter.string = "normal";
    bot.alive = false;
    chatter.AddStatement(new BotStatement(1.0f, true), false);
    assert(0 == g_liveStatements);
    assert(nullptr == chatter.m_statementList);

    BotStatement* mustSpeakStmt = new BotStatement(3.0f, false);
    chatter.AddStatement(mustSpeakStmt, true);
    assert(1 == g_liveStatements);
    assert(mustSpeakStmt == chatter.m_statementList);
    chatter.ClearAll();
    assert(0 == g_liveStatements);

    // 5. Normal alive bot: empty and redundant statements are deleted; valid statements are queued in order.
    bot.alive = true;
    chatter.AddStatement(new BotStatement(1.0f, false, 0), false);
    assert(0 == g_liveStatements);

    BotStatement* second = new BotStatement(5.0f, false);
    BotStatement* first = new BotStatement(2.0f, false);
    chatter.AddStatement(second, false);
    chatter.AddStatement(first, false);
    chatter.AddStatement(new BotStatement(3.0f, false, 1, true), false);
    assert(2 == g_liveStatements);
    assert(first == chatter.m_statementList);
    assert(second == first->m_next);
    assert(first == second->m_prev);
    assert(nullptr == second->m_next);

    chatter.ClearAll();
    assert(0 == g_liveStatements);
    return 0;
}
'''


def main():
    production = extract(
        'cstrike/dlls/bot/cs_bot_chatter.cpp',
        'void BotChatterInterface::AddStatement( BotStatement *statement, bool mustSpeak )',
    )
    with tempfile.TemporaryDirectory(prefix='goldsrc-cstrike-bot-chatter-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: BotChatterInterface::AddStatement deletes statements on all early-return paths')


if __name__ == '__main__':
    main()
