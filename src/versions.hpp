#pragma once
#include <QString>

// Two numbers (t4c0, 2026-11-08). ROADIE_SIGNAL_VERSION in this core is the Roadie connector: the
// part the dock's Update button downloads. The OBS plugin is the loader people install by hand, and
// it keeps its own number, changing only when a change needs a reinstall.

// This core's own number, the Roadie connector.
QString connectorVersion();

// The loader's number, read from the description it gives OBS ("Roadie Signal 1.0.0 (loader)"), so
// it works with every loader already installed. Empty when no Roadie loader is found.
QString loaderVersion();
