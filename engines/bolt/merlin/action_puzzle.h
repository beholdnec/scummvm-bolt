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

#ifndef BOLT_MERLIN_ACTION_PUZZLE_H
#define BOLT_MERLIN_ACTION_PUZZLE_H

#include "bolt/bolt.h"
#include "bolt/merlin/merlin.h"

namespace Bolt {
	
class ActionPuzzle : public Card {
public:
	static Card* make(Graphics *graphics, Boltlib &boltlib, BltId resId);
	void init(Graphics *graphics, Boltlib &boltlib, BltId resId);
	void enter();
	Signal handleEvent(const BoltEvent &event);
protected:
	Graphics *_graphics;
	BltImage _bgImage;
	BltPalette _palette;
};

} // End of namespace Bolt

#endif