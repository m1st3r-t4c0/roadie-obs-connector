#include "versions.hpp"
#include <obs-module.h>
#include <QRegularExpression>

QString connectorVersion() { return QString::fromLatin1(ROADIE_SIGNAL_VERSION); }

QString loaderVersion()
{
  // Modules never change after OBS starts, so the answer is read once.
  static const QString found = [] {
    QString version;
    obs_enum_modules(
        [](void *param, obs_module_t *module) {
          auto *out = static_cast<QString *>(param);
          if (!out->isEmpty())
            return;
          const char *description = obs_get_module_description(module);
          if (!description)
            return;
          static const QRegularExpression pattern(QStringLiteral(R"(^Roadie Signal (\d+\.\d+\.\d+) \(loader\)$)"));
          const auto match = pattern.match(QString::fromUtf8(description));
          if (match.hasMatch())
            *out = match.captured(1);
        },
        &version);
    return version;
  }();
  return found;
}
