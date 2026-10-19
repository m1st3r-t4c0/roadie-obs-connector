#include "signal-dock.hpp"
#include "scene-events.hpp"
#include <browser-panel.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QAction>
#include <QHBoxLayout>
#include <QStackedLayout>
#include <QPushButton>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QStringList>
#include <QVBoxLayout>
#include <memory>
#include <QVersionNumber>

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description()
{
  return "Roadie Signal " ROADIE_SIGNAL_VERSION " native dock with a local OBS audio bridge";
}

namespace {
// Which Roadie servers OBS talks to. Each is a bit, so "both" is the two OR'd.
enum Server : int { Production = 1, Staging = 2, Both = Production | Staging };

// One Roadie page per server, in the one Roadie Signal dock. t4c0, 2026-10-11: "we dont need two
// tabs ... when production and staging is on in the plugin menu should be all we need". The dock
// shows one page: production's, or staging's when staging is the only server. Any other page runs
// out of sight behind it, signs itself in, and shares because the plugin menu said so.
struct Slot {
  Server server;
  const char *title;
  QPointer<SignalDock> dock;
  const char *status;
};
Slot docks[] = {
    {Production, "Production", nullptr, "Off"},
    {Staging, "Staging", nullptr, "Off"},
};

constexpr const char *dockId = "roadie-signal";
// Production's store, kept so the existing sign-in survives. Every page shares it: Roadie's
// session cookie is per site, so the servers never see each other's, and the Twitch login they
// share is what lets a page out of sight sign itself in without a click.
constexpr const char *cookieStore = "roadie-signal/browser-cookies";
std::unique_ptr<QCefCookieManager> cookies;
// The pages are stacked with StackAll, so the page behind stays a visible widget: its browser
// loads and keeps sending, it is only covered.
QPointer<QWidget> host;
QPointer<QWidget> pagesHost;
QPointer<QStackedLayout> pages;
// A page brought forward for a sign-in Twitch would not finish by itself (a first approval).
QPointer<QWidget> signInBar;
QPointer<QLabel> signInLabel;
int signingIn = -1;
bool signingInLeftDock = false;

std::unique_ptr<QCef> browser;
std::unique_ptr<SceneEvents> sceneEvents;
QPointer<QAction> settingsAction;
const char *browserStatus = "Dock startup has not run";
int sendTo = Production;
int acceptFrom = Production;

QString originOf(Server server)
{
  return QString::fromUtf8(server == Production ? ROADIE_SIGNAL_ORIGIN : ROADIE_SIGNAL_STAGING_ORIGIN);
}

bool stagingBuilt() { return !originOf(Staging).isEmpty(); }

int validChoice(long long value) { return value == Staging || value == Both ? int(value) : Production; }

void loadServers()
{
  char *path = obs_module_config_path("settings.json");
  obs_data_t *settings = path ? obs_data_create_from_json_file(path) : nullptr;
  bfree(path);
  if (settings) {
    sendTo = validChoice(obs_data_get_int(settings, "sendTo"));
    acceptFrom = validChoice(obs_data_get_int(settings, "acceptFrom"));
    obs_data_release(settings);
  }
  // A build without a staging origin can only ever talk to production.
  if (!stagingBuilt())
    sendTo = acceptFrom = Production;
}

void saveServers()
{
  char *path = obs_module_config_path("settings.json");
  if (!path)
    return;
  // Read, change, write: settings.json also holds the microphone selection.
  obs_data_t *settings = obs_data_create_from_json_file(path);
  if (!settings)
    settings = obs_data_create();
  obs_data_set_int(settings, "sendTo", sendTo);
  obs_data_set_int(settings, "acceptFrom", acceptFrom);
  if (!obs_data_save_json_safe(settings, path, "tmp", "bak"))
    blog(LOG_WARNING, "[roadie-signal] server choice could not be saved");
  obs_data_release(settings);
  bfree(path);
}

template <typename Fn> void eachDock(Fn fn)
{
  for (Slot &slot : docks)
    if (slot.dock)
      fn(slot.dock.data());
}

int frontIndex()
{
  if (signingIn >= 0 && docks[signingIn].dock)
    return signingIn;
  for (int i = 0; i < 2; ++i) {
    if (docks[i].dock)
      return i;
  }
  return -1;
}

// Puts the right page in front and lets every other one run, and sign in, out of sight.
void refreshFront()
{
  if (signingIn >= 0 && !docks[signingIn].dock)
    signingIn = -1;
  const int front = frontIndex();
  for (int i = 0; i < 2; ++i) {
    if (docks[i].dock)
      docks[i].dock->setBackground(i != front);
  }
  if (pages && front >= 0)
    pages->setCurrentWidget(docks[front].dock);
  if (signInBar)
    signInBar->setVisible(signingIn >= 0);
  if (signingIn >= 0 && signInLabel)
    signInLabel->setText(QString("Signing in to Roadie %1. This closes by itself once you are back on Roadie.")
                             .arg(docks[signingIn].title));
}

void showSignIn(int index)
{
  if (!docks[index].dock)
    return;
  signingIn = index;
  signingInLeftDock = false;
  refreshFront();
}

void finishSignIn()
{
  signingIn = -1;
  signingInLeftDock = false;
  refreshFront();
}

// A page brought forward goes back out of sight once it has left /dock for Twitch and returned.
void pageChanged(int index)
{
  if (index != signingIn || !docks[index].dock)
    return;
  if (!docks[index].dock->onDock())
    signingInLeftDock = true;
  else if (signingInLeftDock)
    finishSignIn();
}

QString pageStatus(int index)
{
  const Slot &slot = docks[index];
  if (!slot.dock)
    return QString::fromUtf8(slot.status);
  const QString elsewhere = slot.dock->elsewhere();
  if (!elsewhere.isEmpty())
    return QString("Signing in (at %1)").arg(elsewhere);
  return index == frontIndex() ? QString("Running, shown in the dock") : QString("Running out of sight");
}

bool createHost()
{
  if (host)
    return true;
  host = new QWidget;
  auto *layout = new QVBoxLayout(host);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  signInBar = new QWidget(host);
  auto *bar = new QHBoxLayout(signInBar);
  bar->setContentsMargins(6, 4, 6, 2);
  signInLabel = new QLabel(signInBar);
  signInLabel->setWordWrap(true);
  auto *done = new QPushButton("Done", signInBar);
  QObject::connect(done, &QPushButton::clicked, [] { finishSignIn(); });
  bar->addWidget(signInLabel, 1);
  bar->addWidget(done);
  pagesHost = new QWidget(host);
  pages = new QStackedLayout(pagesHost);
  pages->setStackingMode(QStackedLayout::StackAll);
  layout->addWidget(signInBar);
  layout->addWidget(pagesHost, 1);
  signInBar->setVisible(false);
  if (!obs_frontend_add_dock_by_id(dockId, "Roadie Signal", host)) {
    delete host.data();
    browserStatus = "OBS rejected dock registration";
    blog(LOG_WARNING, "[roadie-signal] dock registration failed");
    return false;
  }
  blog(LOG_INFO, "[roadie-signal] Roadie Signal dock registered");
  return true;
}

bool startSlot(Slot &slot)
{
  if (slot.dock)
    return true;
  if (!browser || !cookies || !createHost()) {
    slot.status = "Unavailable: the dock did not start";
    return false;
  }
  slot.dock = new SignalDock(browser.get(), originOf(slot.server), cookies.get(), pagesHost);
  if (!slot.dock->initialized()) {
    delete slot.dock.data();
    slot.status = "Page initialization failed; check the OBS log";
    blog(LOG_WARNING, "[roadie-signal] %s page initialization failed", slot.title);
    return false;
  }
  pages->addWidget(slot.dock);
  const int index = &slot == &docks[0] ? 0 : 1;
  QObject::connect(slot.dock, &SignalDock::pageChanged, [index] { pageChanged(index); });
  slot.status = "Running";
  blog(LOG_INFO, "[roadie-signal] %s page started", slot.title);
  refreshFront();
  return true;
}

void stopSlot(Slot &slot)
{
  if (!slot.dock)
    return;
  // Close CEF while the complete widget tree is alive, then delete the page.
  // Starting CEF close from the destructor permits nested paint events to
  // visit a partially destroyed Qt tree.
  slot.dock->prepareForShutdown();
  if (pages)
    pages->removeWidget(slot.dock);
  if (slot.dock)
    delete slot.dock.data();
  slot.status = "Off";
  refreshFront();
}

// Opens the docks the choice needs, closes the rest, and sets each dock's roles.
void applyServers()
{
  for (Slot &slot : docks) {
    const bool sends = (sendTo & slot.server) != 0;
    const bool obeys = (acceptFrom & slot.server) != 0;
    if (!sends && !obeys) {
      stopSlot(slot);
      continue;
    }
    if (startSlot(slot))
      slot.dock->setRoles(sends, obeys);
  }
}

const char *describe(int choice)
{
  return choice == Both ? "Production and staging" : choice == Staging ? "Staging" : "Production";
}

QString savedMicrophone()
{
  char *path = obs_module_config_path("settings.json");
  obs_data_t *settings = path ? obs_data_create_from_json_file(path) : nullptr;
  bfree(path);
  if (!settings)
    return QString();
  const QString uuid = QString::fromUtf8(obs_data_get_string(settings, "microphoneUuid"));
  obs_data_release(settings);
  return uuid;
}

void saveMicrophone(const QString &uuid)
{
  char *path = obs_module_config_path("settings.json");
  if (!path)
    return;
  // Read, change, write: settings.json also holds the server choices.
  obs_data_t *settings = obs_data_create_from_json_file(path);
  if (!settings)
    settings = obs_data_create();
  obs_data_set_string(settings, "microphoneUuid", uuid.toUtf8().constData());
  if (!obs_data_save_json_safe(settings, path, "tmp", "bak"))
    blog(LOG_WARNING, "[roadie-signal] microphone selection could not be saved");
  obs_data_release(settings);
  bfree(path);
}

bool addAudioSource(void *param, obs_source_t *source)
{
  if (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) {
    auto *choices = static_cast<QComboBox *>(param);
    choices->addItem(QString::fromUtf8(obs_source_get_name(source)),
                     QString::fromUtf8(obs_source_get_uuid(source)));
  }
  return true;
}

// Other Roadie Signal DLLs OBS loaded beside this one, usually an older copy left
// in the plugin folder under another name. Each adds its own Tools entries and
// competes for the same dock, so the user is told which file to delete.
QStringList otherRoadieModules()
{
  QStringList found;
  obs_enum_modules(
      [](void *param, obs_module_t *module) {
        if (module == obs_current_module())
          return;
        const char *description = obs_get_module_description(module);
        const char *file = obs_get_module_file_name(module);
        const bool roadie = (description && QString::fromUtf8(description).startsWith("Roadie Signal")) ||
                            (file && QString::fromUtf8(file).contains("roadie-signal", Qt::CaseInsensitive));
        if (!roadie)
          return;
        const char *path = obs_get_module_binary_path(module);
        static_cast<QStringList *>(param)->append(QString::fromUtf8(path ? path : file ? file : "unknown"));
      },
      &found);
  return found;
}

QComboBox *serverChoice(QWidget *parent, int current)
{
  auto *choices = new QComboBox(parent);
  for (int choice : {int(Production), int(Staging), int(Both)})
    choices->addItem(describe(choice), choice);
  choices->setCurrentIndex(choices->findData(current));
  return choices;
}

// The plugin's one settings window: what is running, the microphone, and the servers.
void openSettings()
{
  QDialog dialog;
  dialog.setWindowTitle("Roadie Signal");
  auto *layout = new QVBoxLayout(&dialog);

  QString text = QString("Roadie Signal %1 · OBS %2 · Qt %3\nBrowser: %4\nProduction: %5")
                     .arg(ROADIE_SIGNAL_VERSION, obs_get_version_string(), qVersion(), browserStatus,
                          pageStatus(0));
  if (stagingBuilt())
    text += QString("\nStaging: %1").arg(pageStatus(1));
  auto *status = new QLabel(text, &dialog);
  status->setTextFormat(Qt::PlainText);
  status->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(status);
  // A page out of sight signs itself in through Twitch. Twitch asks for a click only the first
  // time an account approves a Roadie server; this brings that page forward for it.
  for (int i = 0; i < 2; ++i) {
    if (!docks[i].dock || i == frontIndex())
      continue;
    auto *signIn = new QPushButton(QString("Show the %1 sign-in").arg(docks[i].title), &dialog);
    signIn->setToolTip("Only needed if this server never signs in by itself.");
    QObject::connect(signIn, &QPushButton::clicked, &dialog, [i, &dialog] {
      showSignIn(i);
      dialog.reject();
    });
    layout->addWidget(signIn, 0, Qt::AlignLeft);
  }

  const QStringList others = otherRoadieModules();
  if (!others.isEmpty()) {
    auto *duplicate = new QLabel(
        QString("Another Roadie Signal plugin is also loaded. Close OBS and delete:\n%1").arg(others.join("\n")),
        &dialog);
    duplicate->setTextFormat(Qt::PlainText);
    duplicate->setWordWrap(true);
    duplicate->setTextInteractionFlags(Qt::TextSelectableByMouse);
    duplicate->setStyleSheet("color: #e5534b;");
    layout->addWidget(duplicate);
  }

  auto *form = new QFormLayout;
  auto *microphone = new QComboBox(&dialog);
  microphone->addItem("None — microphone sharing off", "");
  obs_enum_sources(addAudioSource, microphone);
  const QString currentMicrophone = savedMicrophone();
  int selected = microphone->findData(currentMicrophone);
  if (selected < 0 && !currentMicrophone.isEmpty()) {
    microphone->addItem("Selected source unavailable", currentMicrophone);
    selected = microphone->count() - 1;
  }
  microphone->setCurrentIndex(selected < 0 ? 0 : selected);
  form->addRow("Microphone level from:", microphone);

  QComboBox *send = nullptr;
  QComboBox *accept = nullptr;
  if (stagingBuilt()) {
    send = serverChoice(&dialog, sendTo);
    accept = serverChoice(&dialog, acceptFrom);
    form->addRow("Send OBS data to:", send);
    form->addRow("Accept OBS changes from:", accept);
  }
  layout->addLayout(form);

  auto *hint = new QLabel(&dialog);
  hint->setWordWrap(true);
  const auto explain = [send, accept, hint] {
    QString help = "Roadie reads only the level of the microphone source, never its audio.";
    if (send && accept) {
      help += " OBS data is your scene, stream and recording state, microphone level, stream health and "
              "crash reports. The dock shows Production; with both on, Staging runs out of sight, signs in "
              "with your Twitch login and shares by itself.";
      const int s = send->currentData().toInt();
      const int a = accept->currentData().toInt();
      if (a & Staging)
        help += "\n\nStaging will be able to change your live OBS: scenes, sources, text, audio and filters.";
      if ((a & ~s) != 0)
        help += "\n\nA server that gets no OBS data cannot see your scenes and sources, so Roadie there will not "
                "offer OBS changes.";
    }
    hint->setText(help);
  };
  if (send && accept) {
    QObject::connect(send, &QComboBox::currentIndexChanged, hint, explain);
    QObject::connect(accept, &QComboBox::currentIndexChanged, hint, explain);
  }
  explain();
  layout->addWidget(hint);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  if (dialog.exec() != QDialog::Accepted)
    return;

  const QString chosenMicrophone = microphone->currentData().toString();
  if (chosenMicrophone != currentMicrophone) {
    saveMicrophone(chosenMicrophone);
    eachDock([](SignalDock *each) { each->reloadMicrophone(); });
  }
  if (send && accept) {
    sendTo = send->currentData().toInt();
    acceptFrom = accept->currentData().toInt();
    saveServers();
    if (!browser)
      return; // The dialog already says why no dock runs.
    applyServers();
    for (const Slot &slot : docks) {
      if (((sendTo | acceptFrom) & slot.server) && !slot.dock)
        QMessageBox::warning(nullptr, "Roadie Signal",
                             QString("The %1 page could not start: %2").arg(slot.title, slot.status));
    }
  }
}

void addMenuAction()
{
  // Added even when the dock fails to start, so the dialog can say why.
  if (settingsAction)
    return;
  settingsAction = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction("Roadie Signal…"));
  if (settingsAction)
    QObject::connect(settingsAction, &QAction::triggered, [] { openSettings(); });
}

void warnAboutOtherCopies()
{
  const QStringList others = otherRoadieModules();
  if (others.isEmpty())
    return;
  for (const QString &path : others)
    blog(LOG_WARNING, "[roadie-signal] another Roadie Signal plugin is loaded: %s", path.toUtf8().constData());
  QMessageBox::warning(nullptr, "Roadie Signal",
                       QString("Another Roadie Signal plugin is loaded beside %1. Two copies compete for the same "
                               "dock and add their own Tools entries.\n\nClose OBS and delete:\n%2")
                           .arg(ROADIE_SIGNAL_VERSION, others.join("\n")));
}

void startDocks()
{
  if (browser)
    return;
  // The matching upstream helper looks up the module by its binary identity,
  // rather than the human-readable name returned by obs_get_module_name.
  if (obs_browser_qcef_version() != 3) {
    browserStatus = "Browser panel API version 3 is unavailable";
    blog(LOG_WARNING, "[roadie-signal] matching browser panel API version 3 required");
    return;
  }
  browser.reset(obs_browser_init_panel());
  if (browser)
    browser->init_browser(); // may already be initialized by OBS
  if (!browser || !browser->wait_for_browser_init()) {
    browser.reset();
    browserStatus = "Browser initialization failed";
    blog(LOG_WARNING, "[roadie-signal] browser initialization failed");
    return;
  }
  // QCef prefixes this name with obs-browser's config path. Passing an absolute
  // path creates an invalid nested drive path on Windows and returns nullptr.
  cookies.reset(browser->create_cookie_manager(cookieStore, true));
  if (!cookies) {
    browserStatus = "Browser cookie store could not be created";
    blog(LOG_WARNING, "[roadie-signal] browser cookie manager could not be created");
    return;
  }
  browserStatus = "Ready";
  loadServers();
  applyServers();
  blog(LOG_INFO, "[roadie-signal] sends OBS data to %s; accepts OBS changes from %s", describe(sendTo),
       describe(acceptFrom));
}

void stopDocks()
{
  // closeBrowser runs a nested event loop; a second shutdown callback must
  // not remove the parent while the first close is still in progress.
  static bool stopping = false;
  if (stopping)
    return;
  stopping = true;
  sceneEvents.reset();
  if (settingsAction)
    delete settingsAction.data();
  // Every browser closes first, with the whole dock alive; then OBS lets go of the
  // dock, and deleting it takes the closed pages with it.
  for (Slot &slot : docks) {
    if (slot.dock)
      slot.dock->prepareForShutdown();
  }
  if (host) {
    obs_frontend_remove_dock(dockId);
    if (host)
      delete host.data();
  }
  for (Slot &slot : docks)
    slot.status = "Off";
  if (cookies)
    cookies->FlushStore();
  cookies.reset();
  browser.reset();
  stopping = false;
}

void frontendEvent(enum obs_frontend_event event, void *)
{
  if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
    addMenuAction();
    startDocks();
    warnAboutOtherCopies();
    sceneEvents = std::make_unique<SceneEvents>();
  }
  if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED && sceneEvents)
    QMetaObject::invokeMethod(sceneEvents.get(), [] {
      if (sceneEvents) sceneEvents->currentScene(true);
    }, Qt::QueuedConnection);
  switch (event) {
  case OBS_FRONTEND_EVENT_SCENE_CHANGED:
  case OBS_FRONTEND_EVENT_STREAMING_STARTED:
  case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
  case OBS_FRONTEND_EVENT_RECORDING_STARTED:
  case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
  case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
  case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
    // Each dock drops the frame itself when its server is not sent OBS data.
    eachDock([](SignalDock *each) {
      QMetaObject::invokeMethod(each, [target = QPointer<SignalDock>(each)] {
        if (target) target->publishState();
      }, Qt::QueuedConnection);
    });
    break;
  default: break;
  }
  // The output still exists while stopping: take the night's last health sample.
  if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPING)
    eachDock([](SignalDock *each) { each->publishHealth(); });
  if (event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN)
    stopDocks();
}
}

bool obs_module_load()
{
  if (obs_get_version() < MAKE_SEMANTIC_VERSION(30, 0, 0)) {
    blog(LOG_WARNING, "[roadie-signal] OBS 30.0.0 or newer required");
    return false;
  }
  const auto runtimeQt = QVersionNumber::fromString(QString::fromLatin1(qVersion()));
  const QVersionNumber minimumQt(QT_VERSION_MAJOR, QT_VERSION_MINOR, QT_VERSION_PATCH);
  if (runtimeQt.majorVersion() != QT_VERSION_MAJOR || runtimeQt < minimumQt) {
    blog(LOG_WARNING, "[roadie-signal] compatible Qt 6.5.3 or newer required; runtime is %s", qVersion());
    return false;
  }
  blog(LOG_INFO, "[roadie-signal] native dock %s; OBS %s; Qt %s; baseline OBS 30.0.0 / Qt 6.5.3",
       ROADIE_SIGNAL_VERSION, obs_get_version_string(), qVersion());
  obs_frontend_add_event_callback(frontendEvent, nullptr);
  return true;
}

void obs_module_unload()
{
  obs_frontend_remove_event_callback(frontendEvent, nullptr);
  stopDocks();
}
