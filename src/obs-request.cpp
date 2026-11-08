#include "obs-request.hpp"
#include "../third_party/obs-websocket-api.h"
#include <obs-module.h>
#include <QByteArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QString>

namespace {
// Bounds, so one answer cannot grow the page's frame past what the bridge carries comfortably.
constexpr int maxRequestBytes = 64 * 1024;
constexpr int maxResponseBytes = 512 * 1024;

// The block list (t4c0, 2026-11-08: "everything except quitting OBS, profiles, and stream settings
// and keys"). obs-websocket has no request that quits OBS. Added to the list because each reaches
// past OBS's scenes and sources: switching or editing scene collections (it replaces the whole
// setup), writing files at a path Roadie names, the canvas and output size, and other plugins'
// vendor requests, whose effects this list cannot see.
const QSet<QString> &blocked()
{
  static const QSet<QString> list{
      // Profiles.
      QStringLiteral("GetProfileList"), QStringLiteral("SetCurrentProfile"), QStringLiteral("CreateProfile"),
      QStringLiteral("RemoveProfile"), QStringLiteral("GetProfileParameter"),
      QStringLiteral("SetProfileParameter"),
      // Scene collections.
      QStringLiteral("SetCurrentSceneCollection"), QStringLiteral("CreateSceneCollection"),
      // The stream service: server, key, and the settings around them.
      QStringLiteral("GetStreamServiceSettings"), QStringLiteral("SetStreamServiceSettings"),
      // Canvas and output size.
      QStringLiteral("SetVideoSettings"),
      // Paths on disk.
      QStringLiteral("SetRecordDirectory"), QStringLiteral("SaveSourceScreenshot"),
      // Other plugins.
      QStringLiteral("CallVendorRequest")};
  return list;
}

QJsonObject failed(const char *code) { return QJsonObject{{"ok", false}, {"error", QString::fromLatin1(code)}}; }
} // namespace

QJsonArray obsRequestBlocked()
{
  QJsonArray out;
  for (const QString &name : blocked())
    out.append(name);
  return out;
}

QJsonObject runObsRequest(const QJsonObject &command)
{
  const QString type = command.value("requestType").toString();
  static const QRegularExpression name(QStringLiteral("^[A-Z][A-Za-z]{1,63}$"));
  if (!name.match(type).hasMatch())
    return failed("bad-command");
  if (blocked().contains(type))
    return failed("blocked");
  const QJsonValue given = command.value("requestData");
  if (!given.isUndefined() && !given.isNull() && !given.isObject())
    return failed("bad-command");
  obs_data_t *data = nullptr;
  if (given.isObject()) {
    const QByteArray json = QJsonDocument(given.toObject()).toJson(QJsonDocument::Compact);
    if (json.size() > maxRequestBytes)
      return failed("too-large");
    data = obs_data_create_from_json(json.constData());
    if (!data)
      return failed("bad-command");
  }
  const QByteArray typeBytes = type.toLatin1();
  obs_websocket_request_response *response = obs_websocket_call_request(typeBytes.constData(), data);
  if (data)
    obs_data_release(data);
  if (!response)
    return failed("obs-websocket-missing");
  QJsonObject out;
  const int status = static_cast<int>(response->status_code);
  // obs-websocket's RequestStatus: 100 is success; anything else is OBS's own refusal.
  if (status == 100) {
    QJsonObject body;
    if (response->response_data) {
      const QByteArray bytes(response->response_data);
      if (bytes.size() > maxResponseBytes) {
        obs_websocket_request_response_free(response);
        return failed("too-large");
      }
      body = QJsonDocument::fromJson(bytes).object();
    }
    out = QJsonObject{{"ok", true}, {"status", status}, {"response", body}};
  } else {
    out = failed("request-failed");
    out["status"] = status;
    if (response->comment)
      out["comment"] = QString::fromUtf8(response->comment).left(500);
  }
  obs_websocket_request_response_free(response);
  return out;
}
