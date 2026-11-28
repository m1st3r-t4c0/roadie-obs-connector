#pragma once
// #394: an OBS crash report, put in the order Roadie needs to read it, inside the size it accepts.
//
// OBS's Windows crash handler writes the exception and versions, then one block per thread (the
// crashed one wherever its id falls), then `Loaded modules:`. A report from an OBS with 50-60
// threads runs past 64 KB, and until 1.1.3 the plugin sent its first 64 KB: the module list never
// arrived, and sometimes not the crashed thread either. So a long report is sent as: the header,
// the crashed thread, the module list, then as many other threads as still fit, and a line saying
// how many were left out. Roadie reads the crashed thread up to the next `Thread ` or
// `Loaded modules:` line, so that order reads the same as the original.
//
// `runLine` (may be empty) goes in right after the first line: which Roadie connector was running
// when OBS crashed, which can differ from the one sending the report after an update.
//
// Qt only, so tests/dock-logic runs it without OBS.
#include <QString>
#include <QStringList>

namespace CrashCompact {

struct Result {
  QString text;
  bool cut = false; // something was left out to fit
};

inline bool isThreadHeader(const QString &line) { return line.startsWith(QLatin1String("Thread ")); }
inline bool isModulesHeader(const QString &line) { return line.startsWith(QLatin1String("Loaded modules:")); }

inline Result compact(const QString &text, int maxChars, const QString &runLine = QString())
{
  const bool crlf = text.contains(QLatin1String("\r\n"));
  const QString eol = crlf ? QStringLiteral("\r\n") : QStringLiteral("\n");
  QStringList lines = text.split(eol);

  // Sections: header (before the first thread), thread blocks, the module list.
  QStringList header;
  QList<QStringList> threads;
  QStringList modules;
  int crashed = -1;
  int section = 0; // 0 header, 1 threads, 2 modules
  for (const QString &line : lines) {
    if (section < 2 && isModulesHeader(line)) {
      section = 2;
    } else if (section < 2 && isThreadHeader(line)) {
      section = 1;
      threads.append(QStringList());
      if (crashed < 0 && line.contains(QLatin1String("(Crashed)")))
        crashed = int(threads.size()) - 1;
    }
    if (section == 0)
      header.append(line);
    else if (section == 1)
      threads.last().append(line);
    else
      modules.append(line);
  }
  if (!runLine.isEmpty())
    header.insert(header.isEmpty() ? 0 : 1, runLine);

  QStringList all = header;
  for (const QStringList &t : threads)
    all += t;
  all += modules;
  const QString full = all.join(eol);
  if (full.size() <= maxChars)
    return {full, false};

  // Too long: header, crashed thread, modules, then the other threads in their own order, as many
  // as fit (stopping at the first that does not, so the ones kept are the first ones).
  QStringList out = header;
  if (crashed >= 0)
    out += threads[crashed];
  out += modules;
  const auto size = [&](const QStringList &l) { return int(l.join(eol).size()) + int(eol.size()); };
  int used = size(out);
  const int others = int(threads.size()) - (crashed >= 0 ? 1 : 0);
  int keptThreads = 0;
  for (int i = 0; i < threads.size(); ++i) {
    if (i == crashed)
      continue;
    const int need = size(threads[i]);
    if (used + need + 120 > maxChars) // room for the closing line
      break;
    out += threads[i];
    used += need;
    ++keptThreads;
  }
  const int dropped = others - keptThreads;
  if (dropped > 0)
    out.append(QStringLiteral("[Roadie: %1 other thread%2 left out to fit %3 KB]")
                   .arg(dropped)
                   .arg(dropped == 1 ? QString() : QStringLiteral("s"))
                   .arg(maxChars / 1024));
  QString result = out.join(eol);
  if (result.size() > maxChars)
    result.truncate(maxChars); // header, crashed thread and modules alone too long: rare
  return {result, true};
}

} // namespace CrashCompact
