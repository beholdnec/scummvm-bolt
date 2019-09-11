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

#include "funhouse/merlin/memory_puzzle.h"

#include "funhouse/boltlib/boltlib.h"

namespace Funhouse {

struct BltMemoryPuzzleInfo {
    static const uint32 kType = kBltMemoryPuzzleInfos;
    static const uint kSize = 0x10;
    void load(Common::Span<const byte> src, Boltlib &boltlib) {
        maxMemorize = src.getUint16BEAt(2);
        // TODO: the rest of the fields appear to be timing parameters
    }

    uint16 maxMemorize; // Number of items to memorize for this difficulty level
};

typedef ScopedArray<BltMemoryPuzzleInfo> BltMemoryPuzzleInfos;

struct BltMemoryPuzzleItem {
	static const uint32 kType = kBltMemoryPuzzleItemList;
	static const uint kSize = 0x10;
	void load(Common::Span<const byte> src, Boltlib &boltlib) {
		numFrames = src.getUint16BEAt(0);
		framesId = BltId(src.getUint32BEAt(2)); // Ex: 8642
		paletteId = BltId(src.getUint32BEAt(6)); // Ex: 861D
		colorCyclesId = BltId(src.getUint32BEAt(0xA));
		soundId = BltShortId(src.getUint16BEAt(0xE)); // Ex: 860C
	}

	uint16 numFrames;
	BltId framesId;
	BltId paletteId;
	BltId colorCyclesId;
	BltId soundId;
};

typedef ScopedArray<BltMemoryPuzzleItem> BltMemoryPuzzleItemList;

struct BltMemoryPuzzleItemFrame {
	static const uint32 kType = kBltMemoryPuzzleItemFrameList;
	static const uint kSize = 0xA;
	void load(Common::Span<const byte> src, Boltlib &boltlib) {
		// FIXME: position at 0?
		pos.x = src.getInt16BEAt(0);
		pos.y = src.getInt16BEAt(2);
		imageId = BltId(src.getUint32BEAt(4)); // 8640
        type = src.getInt16BEAt(8);
	}

	Common::Point pos;
	BltId imageId;
    int16 type;
};

typedef ScopedArray<BltMemoryPuzzleItemFrame> BltMemoryPuzzleItemFrameList;

MemoryPuzzle::MemoryPuzzle() : _random("MemoryPuzzleRandomSource")
{}

void MemoryPuzzle::init(MerlinGame *game, Boltlib &boltlib, BltId resId) {
    _game = game;
	_graphics = _game->getGraphics();
    _eventLoop = _game->getEventLoop();
    _animationActive = false;
    _playbackActive = false;
    _matches = 0;

    _popup.init(_game, boltlib, _game->getPopupResId(MerlinGame::kPuzzlePopup));

	BltResourceList resourceList;
	loadBltResourceArray(resourceList, boltlib, resId);
    BltId infosId     = resourceList[0].value; // Ex: 8600
	BltId sceneId     = resourceList[1].value; // Ex: 8606
    BltId failSoundId = resourceList[2].value; // Ex: 8608
    BltId itemsId     = resourceList[3].value; // Ex: 865D

    BltMemoryPuzzleInfos infos;
    loadBltResourceArray(infos, boltlib, infosId);
    _maxMemorize = infos[0].maxMemorize; // TODO: select difficulty
    _curMemorize = 3;

	_scene.load(_game->getEngine(), boltlib, sceneId);

	BltMemoryPuzzleItemList itemList;
	loadBltResourceArray(itemList, boltlib, itemsId);

	_itemList.alloc(itemList.size());
	for (uint i = 0; i < itemList.size(); ++i) {
		BltMemoryPuzzleItemFrameList frames;
		loadBltResourceArray(frames, boltlib, itemList[i].framesId);

		_itemList[i].frames.alloc(frames.size());
		for (uint j = 0; j < frames.size(); ++j) {
            ItemFrame& frame = _itemList[i].frames[j];
            frame.pos = frames[j].pos;
			frame.image.load(boltlib, frames[j].imageId);
            frame.type = (FrameType)frames[j].type;
		}

		_itemList[i].palette.load(boltlib, itemList[i].paletteId);
		if (itemList[i].colorCyclesId.isValid()) {
			_itemList[i].colorCycles.reset(new BltColorCycles);
			loadBltResource(*_itemList[i].colorCycles, boltlib, itemList[i].colorCyclesId);
		}
	}

    _solution.alloc(_maxMemorize);
    for (int i = 0; i < _maxMemorize; ++i) {
        _solution[i] = _random.getRandomNumber(_itemList.size() - 1);
    }

    startPlayback();
}

void MemoryPuzzle::enter() {
	_scene.enter();
}

BoltCmd MemoryPuzzle::handleMsg(const BoltMsg &msg) {
    if (_animationActive) {
        return driveAnimation();
    } else if (_playbackActive) {
        return drivePlayback();
    }
    
    if (_matches >= _maxMemorize) {
        return kWin;
    } else if (_matches >= _curMemorize) {
        _matches = 0;
        _curMemorize += 3;
        startPlayback();
        return BoltCmd::kDone;
    }

    BoltCmd cmd = _popup.handleMsg(msg);
    if (cmd.type != BoltCmd::kPass) {
        return cmd;
    }

    if (msg.type == Scene::kClickButton) {
        return handleButtonClick(msg.num);
    }

    return _scene.handleMsg(msg);
}

BoltCmd MemoryPuzzle::handleButtonClick(int num) {
    debug(3, "Clicked button %d", num);

    if (num >= 0 && num < _itemList.size()) {
        if (_solution[_matches] == num) {
            // Earn a new match
            ++_matches;
            // TODO: Play success sound
            startAnimation(num);
        } else {
            // Mismatch
            _matches = 0;
            // TODO: Play fail sound
            startAnimation(num);
            startPlayback();
        }
    }

    return BoltCmd::kDone;
}

void MemoryPuzzle::startPlayback() {
    _playbackActive = true;
    _playbackStep = 0;
}

BoltCmd MemoryPuzzle::drivePlayback() {
    assert(_playbackActive);

    if (_playbackStep < _curMemorize) {
        startAnimation(_solution[_playbackStep]);
        ++_playbackStep;
        return BoltCmd::kResend;
    }

    _playbackActive = false;
    return BoltCmd::kResend;
}

void MemoryPuzzle::startAnimation(int itemNum) {
    debug(3, "Starting animation for item %d", itemNum);

    _animationActive = true;
    _animationEnding = false;
    _itemToAnimate = itemNum;
    _frameNum = 0;
    _animStartTime = _eventLoop->getEventTime();
    _frameTime = _eventLoop->getEventTime();
    _frameDelay = kAnimPeriod;

    drawItemFrame(_itemToAnimate, _frameNum);

    const Item &item = _itemList[_itemToAnimate];
    //applyPalette(_graphics, kFore, item.palette);
    // XXX: applyPalette doesn't work correctly. Manually apply palette.
    _graphics->setPlanePalette(kFore, &item.palette.data[BltPalette::kHeaderSize],
        0, 128);
    if (item.colorCycles) {
        applyColorCycles(_graphics, kFore, item.colorCycles.get());
    } else {
        _graphics->resetColorCycles();
    }
}

BoltCmd MemoryPuzzle::driveAnimation() {
    assert(_animationActive);

    // 1. Check selection delay
    if (!_animationEnding) {
        uint32 animTime = _eventLoop->getEventTime() - _animStartTime;
        if (animTime >= kSelectionDelay) {
            _animationActive = false;
            enter(); // Redraw the scene
            return BoltCmd::kResend;
        }
    }

    uint32 frameDelta = _eventLoop->getEventTime() - _frameTime;
    if (frameDelta >= _frameDelay) {
        _frameTime += _frameDelay;
        ++_frameNum;

        if (!_animationEnding) {
            if (_frameNum >= _itemList[_itemToAnimate].frames.size()) {
                _frameNum = 0;
            }

            drawItemFrame(_itemToAnimate, _frameNum);

            FrameType frameType = _itemList[_itemToAnimate].frames[_frameNum].type;
            switch (frameType) {
            case kProceed:
                _frameDelay = kAnimPeriod;
                break;
            case kWaitForEnd:
                _frameDelay = kAnimEndingDelay;
                _animationEnding = true;
                break;
            default:
                warning("Unknown frame type %d\n", (int)frameType);
                break;
            }
        } else { // _animationEnding
            if (_frameNum >= _itemList[_itemToAnimate].frames.size()) {
                _animationActive = false;
                enter(); // Redraw the scene
                return BoltCmd::kResend;
            } else {
                drawItemFrame(_itemToAnimate, _frameNum);
            }
        }
    }

    return BoltCmd::kDone;
}

void MemoryPuzzle::drawItemFrame(int itemNum, int frameNum) {
    // Draw frame corresponding to item
    // TODO: implement animation
    const Item &item = _itemList[itemNum];
    const ItemFrame &frame = item.frames[frameNum];
    const Common::Point &origin = _scene.getOrigin();
    _graphics->clearPlane(kFore);
    frame.image.drawAt(_graphics->getPlaneSurface(kFore), frame.pos.x - origin.x, frame.pos.y - origin.y, true);
    _graphics->markDirty();
}

} // End of namespace Funhouse
