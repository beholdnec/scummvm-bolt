/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "funhouse/merlin/difficulty_menu.h"

namespace Funhouse {
    
void DifficultyMenu::init(MerlinGame *game, Graphics *graphics, IBoltEventLoop *eventLoop, Boltlib &boltlib, BltId resId) {
	_game = game;

	_scene.load(eventLoop, graphics, boltlib, resId);
}

void DifficultyMenu::enter() {
  _scene.enter();
}

BoltCmd DifficultyMenu::handleMsg(const BoltMsg &msg) {
	if (msg.type == Scene::kClickButton) {
		return handleButtonClick(msg.num);
	}

	return _scene.handleMsg(msg);
}

BoltCmd DifficultyMenu::handleButtonClick(int num) {
	switch (num) {
	case -1: // No button
		return Card::kEnd;
	default:
		warning("unknown main menu button %d", num);
		return BoltCmd::kDone;
	}
}

} // End of namespace Funhouse
