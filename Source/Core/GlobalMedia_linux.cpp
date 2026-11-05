//
// AirPodsDesktop - AirPods Desktop User Experience Enhancement Program.
// Copyright (C) 2021-2022 SpriteOvO
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "GlobalMedia_linux.h"

#if defined APD_OS_LINUX

#include <mutex>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QStringList>

#include "../Logger.h"

namespace Core::GlobalMedia {

namespace {

constexpr auto kMprisServicePrefix = "org.mpris.MediaPlayer2.";
constexpr auto kMprisPath = "/org/mpris/MediaPlayer2";
constexpr auto kMprisPlayerInterface = "org.mpris.MediaPlayer2.Player";

QStringList MprisServices()
{
    const auto reply = QDBusConnection::sessionBus().interface()->registeredServiceNames();
    if (!reply.isValid()) {
        return {};
    }

    QStringList result;
    for (const auto &service : reply.value()) {
        if (service.startsWith(kMprisServicePrefix)) {
            result.append(service);
        }
    }
    return result;
}

QString PlaybackStatus(const QString &service)
{
    const QDBusInterface player{service, kMprisPath, kMprisPlayerInterface,
                                QDBusConnection::sessionBus()};
    if (!player.isValid()) {
        return {};
    }
    return player.property("PlaybackStatus").toString();
}

void TogglePlayback(const QString &service)
{
    QDBusInterface player{service, kMprisPath, kMprisPlayerInterface,
                          QDBusConnection::sessionBus()};
    if (!player.isValid()) {
        return;
    }
    player.asyncCall("PlayPause");
}

} // namespace

struct Controller::Impl {
    std::mutex mutex;
    QStringList pausedServices;
};

Controller::Controller() : _impl{std::make_unique<Impl>()}
{
}

Controller::~Controller() = default;

void Controller::Pause()
{
    std::lock_guard<std::mutex> lock{_impl->mutex};

    _impl->pausedServices.clear();

    for (const auto &service : MprisServices()) {
        if (PlaybackStatus(service) != QStringLiteral("Playing")) {
            continue;
        }

        LOG(Info, "Pause MPRIS player '{}'.", service.toStdString());
        TogglePlayback(service);
        _impl->pausedServices.append(service);
    }
}

void Controller::Play()
{
    std::lock_guard<std::mutex> lock{_impl->mutex};

    const auto services = MprisServices();
    QStringList stillPaused;

    for (const auto &service : _impl->pausedServices) {
        if (!services.contains(service)) {
            continue;
        }
        if (PlaybackStatus(service) != QStringLiteral("Paused")) {
            continue;
        }

        LOG(Info, "Resume MPRIS player '{}'.", service.toStdString());
        TogglePlayback(service);
        stillPaused.append(service);
    }

    _impl->pausedServices = stillPaused;
}

} // namespace Core::GlobalMedia

#endif
