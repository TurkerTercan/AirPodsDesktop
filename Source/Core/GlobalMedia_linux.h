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

#pragma once

#if !defined APD_OS_LINUX
    #error "This file shouldn't be compiled."
#endif

#include <memory>

#include "../Helper.h"
#include "GlobalMedia_abstract.h"

namespace Core::GlobalMedia {

class Controller final : public Helper::Singleton<Controller>, public Details::ControllerAbstract
{
protected:
    Controller();
    ~Controller() override;
    friend Helper::Singleton<Controller>;

public:
    void Play() override;
    void Pause() override;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace Core::GlobalMedia
