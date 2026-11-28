// #394 / #412: the script gate's leaving state and the crash report's compaction, without OBS.
#include "../../src/crash-compact.hpp"
#include "../../src/script-gate.hpp"
#include <QCoreApplication>
#include <QThread>
#include <cstdio>

static int failures = 0;
static void check(bool ok, const char *what)
{
  std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok)
    ++failures;
}

static void gate()
{
  ScriptGate g;
  check(g.pageReady(), "a gate that never loaded is ready");

  g.loading();
  check(!g.pageReady(), "a load shuts the gate");
  g.titleSeen();
  check(g.pageReady(), "the page's title opens it");

  // #412: the page says it is leaving; nothing opens the gate until the next page.
  g.leaving();
  check(!g.pageReady(), "leaving shuts the gate");
  g.titleSeen(true);
  check(!g.pageReady(), "a command title from the old page keeps it shut");
  g.loading();
  check(!g.pageReady(), "the next page's commit still waits for its title");
  g.titleSeen();
  check(g.pageReady(), "the next page's title opens it");

  // A reload of the same URL brings no urlChanged: the new document's own title opens it.
  g.leaving();
  g.titleSeen(false);
  check(g.pageReady(), "after leaving, a document's own title opens it");

  // The load fallback does not apply while leaving.
  ScriptGate slow;
  slow.loading();
  slow.titleSeen();
  slow.leaving();
  QThread::msleep(ScriptGate::loadFallbackMs + 200);
  check(!slow.pageReady(), "leaving outlasts the 3 second load fallback");

  ScriptGate closed;
  closed.close();
  check(closed.closed() && !closed.open(), "a closed gate stays shut");
}

static QString report(int otherThreads, int framesEach)
{
  QStringList l;
  l << "Unhandled exception: c0000005" << "Date/Time: 2026-11-27, 03:14:46" << "Fault address: 7FFB0 (obs-browser.dll)"
    << "libobs version: 33.0.3 (64-bit)" << "Windows version: 10.0 build 26100" << "";
  for (int t = 0; t < otherThreads; ++t) {
    if (t == otherThreads / 2) {
      l << "Thread E6E10: (Crashed)" << "Stack            EIP              Arg0";
      l << "000000A1 00007FFB obs-browser.dll!QCefWidgetInternal::executeJavaScript+0x4a";
      l << "000000A2 00007FFB roadie-signal-core.dll!0x7ffba22aec9d" << "";
    }
    l << QStringLiteral("Thread %1: ").arg(1000 + t) << "Stack            EIP              Arg0";
    for (int f = 0; f < framesEach; ++f)
      l << QStringLiteral("0000%1 00007FF8 ntdll.dll!NtWaitForSingleObject+0x14 padding padding padding").arg(f);
    l << "";
  }
  l << "Loaded modules:" << "Base Address      Module" << "00007FF8 obs64.exe" << "00007FFB roadie-signal-core.dll"
    << "00007FFB obs-browser.dll";
  return l.join("\r\n");
}

static void compaction()
{
  const QString small = report(3, 2);
  const CrashCompact::Result same = CrashCompact::compact(small, 64 * 1024);
  check(same.text == small && !same.cut, "a short report is sent as it is");

  const CrashCompact::Result withRun = CrashCompact::compact(small, 64 * 1024, "Roadie connector at crash: 1.1.2 (OBS plugin 1.0.1)");
  check(withRun.text.startsWith("Unhandled exception: c0000005\r\nRoadie connector at crash: 1.1.2"),
        "the run line follows the exception line");

  const QString big = report(60, 30);
  check(big.size() > 64 * 1024, "the long fixture is over 64 KB");
  const CrashCompact::Result cut = CrashCompact::compact(big, 64 * 1024, "Roadie connector at crash: 1.1.2 (OBS plugin 1.0.1)");
  check(cut.cut && cut.text.size() <= 64 * 1024, "a long report fits in 64 KB");
  const int crashed = cut.text.indexOf("(Crashed)");
  const int modules = cut.text.indexOf("Loaded modules:");
  const int other = cut.text.indexOf("Thread 1000: ");
  check(crashed > 0 && modules > crashed, "the crashed thread comes before the module list");
  check(cut.text.contains("roadie-signal-core.dll!0x7ffba22aec9d"), "the crashed thread's frames are kept");
  check(cut.text.contains("00007FFB obs-browser.dll"), "the whole module list is kept");
  check(other > modules, "other threads follow the modules");
  check(cut.text.contains("other threads left out to fit 64 KB]"), "it says how many threads were left out");
  check(cut.text.contains("libobs version: 33.0.3"), "the header is kept");
  // Roadie reads the crashed thread up to the next Thread or Loaded modules line.
  const QString crashedBlock = cut.text.mid(crashed, modules - crashed);
  check(!crashedBlock.contains("\r\nThread "), "nothing else sits inside the crashed thread's block");
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  gate();
  compaction();
  std::printf("%d failure(s)\n", failures);
  return failures ? 1 : 0;
}
