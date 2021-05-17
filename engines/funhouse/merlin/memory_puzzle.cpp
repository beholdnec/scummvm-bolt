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
        finalGoal = src.getUint16BEAt(2);
        // TODO: the rest of the fields appear to be timing parameters
        foo = src.getUint16BEAt(8);
    }

    uint16 finalGoal; // Number of matches to win
    uint16 foo;
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
        delayFrames = src.getInt16BEAt(8);
	}

	Common::Point pos;
	BltId imageId;
    int16 delayFrames;
};

typedef ScopedArray<BltMemoryPuzzleItemFrame> BltMemoryPuzzleItemFrameList;

MemoryPuzzle::MemoryPuzzle() : _random("MemoryPuzzleRandomSource")
{}

void MemoryPuzzle::init(MerlinGame *game, Boltlib &boltlib, int challengeIdx) {
    _game = game;
    _animStatus = kIdle;
    _playbackActive = false;
    _matches = 0;

    uint16 resId = 0;
    switch (challengeIdx) {
    case 3: resId = 0x865E; break;
    case 11: resId = 0x8797; break;
    case 23: resId = 0x887B; break;
    default: assert(false); break;
    }

    _popup.init(_game, boltlib, _game->getPopupResId(MerlinGame::kPuzzlePopup));

	BltResourceList resourceList;
	loadBltResourceArray(resourceList, boltlib, BltShortId(resId));
    BltId infosId     = resourceList[0].value; // Ex: 8600
	BltId sceneId     = resourceList[1].value; // Ex: 8606
    BltId failSoundId = resourceList[2].value; // Ex: 8608
    BltId itemsId     = resourceList[3].value; // Ex: 865D

    BltMemoryPuzzleInfos infos;
    loadBltResourceArray(infos, boltlib, infosId);
    const BltMemoryPuzzleInfo& info = infos[_game->getDifficulty(kMemoryDifficulty)];
    _finalGoal = info.finalGoal;
    _foo = info.foo;
    _goal = 3;

	loadScene(_scene, _game->getEngine(), boltlib, sceneId);

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
            frame.delayFrames = frames[j].delayFrames;
		}

		_itemList[i].palette.load(boltlib, itemList[i].paletteId);
		if (itemList[i].colorCyclesId.isValid()) {
			_itemList[i].colorCycles.reset(new BltColorCycles);
			loadBltResource(*_itemList[i].colorCycles, boltlib, itemList[i].colorCyclesId);
		}

		_itemList[i].sound.load(boltlib, itemList[i].soundId);
	}

	_failSound.load(boltlib, failSoundId);

    _solution.alloc(_finalGoal);
    for (int i = 0; i < _finalGoal; ++i) {
        _solution[i] = _random.getRandomNumber(_itemList.size() - 1);
    }

    startPlayback();
}

void MemoryPuzzle::enter() {
	_scene.enter();
}

BoltRsp MemoryPuzzle::handleMsg(const BoltMsg &msg) {
    BoltRsp cmd;

    if ((cmd = handleAnimation()) != BoltRsp::kPass) {
        return cmd;
    }

    if ((cmd = handlePlayback()) != BoltRsp::kPass) {
        return cmd;
    }
    
    if (_matches >= _finalGoal) {
        _game->branchWin();
        return BoltRsp::kDone;
    } else if (_matches >= _goal) {
        _matches = 0;
        _goal += 3;
        startPlayback();
        return BoltRsp::kDone;
    }

    if ((cmd = _popup.handleMsg(msg)) != BoltRsp::kPass) {
        return cmd;
    }

	switch (msg.type) {
	case BoltMsg::kPopupButtonClick:
		return handlePopupButtonClick(msg.num);
	case Scene::kClickButton:
		return handleButtonClick(msg.num);
	}

    return _scene.handleMsg(msg);
}

BoltRsp MemoryPuzzle::handlePopupButtonClick(int num) {
	switch (num) {
	case 0: // Return
        _game->branchReturn();
		return BoltRsp::kDone;
	default:
		warning("Unhandled popup button %d", num);
		return BoltRsp::kDone;
	}
}

BoltRsp MemoryPuzzle::handleButtonClick(int num) {
    debug(3, "Clicked button %d", num);

    if (num >= 0 && num < _itemList.size()) {
        if (_solution[_matches] == num) {
            // Earn a new match
            ++_matches;
            startAnimation(num, _itemList[num].sound);
        } else {
            // Mismatch
            _matches = 0;
			_failSound.play(_game->getEngine()->_mixer);
            startAnimation(num, _failSound.pickSound());
            startPlayback();
        }
    }

    return BoltRsp::kDone;
}

void MemoryPuzzle::startPlayback() {
    _playbackActive = true;
    _playbackStep = 0;
}

BoltRsp MemoryPuzzle::handlePlayback() {
    if (!_playbackActive) {
        return BoltRsp::kPass;
    }

    if (_playbackStep < _goal) {
        startAnimation(_solution[_playbackStep], _itemList[_solution[_playbackStep]].sound);
        ++_playbackStep;
        _game->getEngine()->setNextMsg(BoltMsg::kDrive);
        return BoltRsp::kDone;
    }

    _playbackActive = false;
    _game->getEngine()->setNextMsg(BoltMsg::kDrive);
    return BoltRsp::kDone;
}

void MemoryPuzzle::startAnimation(int itemNum, BltSound& sound) {
    debug(3, "Starting animation for item %d", itemNum);

    _animStatus = kPlaying;
    _animItem = itemNum;
    _animFrame = 0;
    _animSubFrame = 0;
    _animStartTime = _game->getEngine()->getEventTime();
    _animSoundTime = sound.getNumSamples() / 22; // This approximation is used by the original engine.
    _animPlayTime = _animSoundTime;
    if (_foo == 0x4d) {
        warning("Overriding animation time for foo 0x4d");
        // Triggered in pots-n-pans-n-vials puzzle
        _animPlayTime = 400;
    }
    else if (_animPlayTime < kMinAnimPlayTimeMs) {
        _animPlayTime = kMinAnimPlayTimeMs;
    }
    _frameTime = _animStartTime;

    drawItemFrame(_animItem, _animFrame);

    Item &item = _itemList[_animItem];
    //applyPalette(_graphics, kFore, item.palette);
    // XXX: applyPalette doesn't work correctly. Manually apply palette.
    _game->getGraphics()->setPlanePalette(kFore, &item.palette.data[BltPalette::kHeaderSize],
        0, 128);
    if (item.colorCycles) {
        applyColorCycles(_game->getGraphics(), kFore, item.colorCycles.get());
    } else {
        _game->getGraphics()->resetColorCycles();
    }
    
    sound.play(_game->getEngine()->_mixer);
}

BoltRsp MemoryPuzzle::handleAnimation() {
    switch (_animStatus) {

    case kIdle:
        return BoltRsp::kPass;

    case kPlaying: {
        const Item& item = _itemList[_animItem];
        const ItemFrame& frame = item.frames[_animFrame];

        uint32 frameElapsed = _game->getEngine()->getEventTime() - _frameTime;
        if (frameElapsed >= kFrameDelayMs) {
            _frameTime += kFrameDelayMs;

            uint32 totalElapsed = _game->getEngine()->getEventTime() - _animStartTime;
            if (totalElapsed >= _animPlayTime) {
                if (frame.delayFrames == -1) {
                    _animFrame++;
                    _animSubFrame = 0;
                    _frameTime = _game->getEngine()->getEventTime();
                    drawItemFrame(_animItem, _animFrame);
                    _animStatus = kWindingDown;
                    debug("winding down animation...");
                }
                else {
                    _animStatus = kStopping;
                }

                _game->getEngine()->setNextMsg(BoltMsg::kDrive);
                return BoltRsp::kDone;
            }
            else {
                if (frame.delayFrames == -1) {
                    // Do not advance frames
                }
                else {
                    ++_animSubFrame;
                    if (_animSubFrame >= frame.delayFrames) {
                        ++_animFrame;
                        if (_animFrame >= item.frames.size()) {
                            _animFrame = 0;
                        }
                        _animSubFrame = 0;
                        drawItemFrame(_animItem, _animFrame);
                    }
                }
            }
        }

        return BoltRsp::kDone;
    }

    case kWindingDown: {
        const Item& item = _itemList[_animItem];

        if (_animFrame >= item.frames.size()) {
            _animStatus = kStopping;
            _game->getEngine()->setNextMsg(BoltMsg::kDrive);
            return BoltRsp::kDone;
        }

        const ItemFrame& frame = item.frames[_animFrame];

        uint32 frameElapsed = _game->getEngine()->getEventTime() - _frameTime;
        if (frameElapsed >= kFrameDelayMs) {
            _frameTime += kFrameDelayMs;

            if (frame.delayFrames != -1) {
                ++_animSubFrame;
                if (_animSubFrame >= frame.delayFrames) {
                    ++_animFrame;
                    _animSubFrame = 0;
                    drawItemFrame(_animItem, _animFrame);
                }
            }
            else {
                _animFrame++;
                _animSubFrame = 0;
                drawItemFrame(_animItem, _animFrame);
            }
        }

        return BoltRsp::kDone;
    }

    case kStopping: {
        uint32 totalElapsed = _game->getEngine()->getEventTime() - _animStartTime;
        if (totalElapsed >= _animSoundTime) {
            drawItemFrame(_animItem, -1);
            _animStatus = kIdle;
            _game->getEngine()->setNextMsg(BoltMsg::kDrive);
        }
        return BoltRsp::kDone;
    }

    default:
        assert(false && "Invalid state");
        _animStatus = kIdle;
        return BoltRsp::kPass;
    }
}

void MemoryPuzzle::drawItemFrame(int itemNum, int frameNum) {
    _game->getGraphics()->clearPlane(kFore);

    const Item &item = _itemList[itemNum];
    if (frameNum >= 0 && frameNum < item.frames.size()) {
        const ItemFrame &frame = item.frames[frameNum];
        const Common::Point &origin = _scene.getOrigin();
        frame.image.drawAt(_game->getGraphics()->getPlaneSurface(kFore), frame.pos.x - origin.x, frame.pos.y - origin.y, true);
    }

    _game->getGraphics()->markDirty();
}

} // End of namespace Funhouse
