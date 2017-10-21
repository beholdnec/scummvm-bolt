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

#include "bolt/merlin/sliding_puzzle.h"

namespace Bolt {

struct BltSlidingPuzzle { // type 44
	static const uint32 kType = kBltSlidingPuzzle;
	static const uint kSize = 0xC;
	void load(const ConstSizedDataView<kSize> src, Boltlib &boltlib) {
		unk1 = src.readUint16BE(0);
		difficulty1 = BltShortId(src.readUint16BE(2));
		unk2 = src.readUint16BE(4);
		difficulty2 = BltShortId(src.readUint16BE(6));
		unk3 = src.readUint16BE(8);
		difficulty3 = BltShortId(src.readUint16BE(0xA));
	}

	uint16 unk1;
	BltShortId difficulty1;
	uint16 unk2;
	BltShortId difficulty2;
	uint16 unk3;
	BltShortId difficulty3;
};

void SlidingPuzzle::init(Graphics *graphics, IBoltEventLoop *eventLoop, Boltlib &boltlib, BltId resId) {
	BltResourceList resourceList;
	loadBltResourceArray(resourceList, boltlib, resId);
	BltSlidingPuzzle slidingPuzzleInfo;
	loadBltResource(slidingPuzzleInfo, boltlib, resourceList[1].value);
	// TODO: select proper difficulty based on player setting
	BltResourceList difficultyInfo;
	loadBltResourceArray(difficultyInfo, boltlib, slidingPuzzleInfo.difficulty1); // Ex: 3A34, 3B34, 3C34

	_scene.load(eventLoop, graphics, boltlib, difficultyInfo[1].value);
}

void SlidingPuzzle::enter() {
	_scene.enter();
}

BoltCmd SlidingPuzzle::handleMsg(const BoltMsg &msg) {
	if (msg.type == Scene::kClickButton) {
		return handleButtonClick(msg.num);
	}

	return _scene.handleMsg(msg);
}

BoltCmd SlidingPuzzle::handleButtonClick(int num) {
	debug(3, "Clicked button %d", num);
	// TODO: implement puzzle
	if (num != -1) {
		return Card::kWin;
	}

	return BoltCmd::kDone;
}

} // End of namespace Bolt
