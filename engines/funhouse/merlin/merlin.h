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

#ifndef FUNHOUSE_MERLIN_MERLIN_H
#define FUNHOUSE_MERLIN_MERLIN_H

#include "funhouse/bolt.h"
#include "funhouse/movie.h"
#include "funhouse/merlin/save.h"

namespace Funhouse {
	
class MerlinGame;
struct PuzzleEntry;

enum DifficultyCategory {
	kWordsDifficulty,
	kShapesDifficulty,
	kActionDifficulty,
	kMemoryDifficulty,
	kLogicDifficulty,
	kNumDifficultyCategories,
};

struct HubEntry {
	uint16 hubId;
	int numPuzzles;
	const PuzzleEntry *puzzles;
};

struct PuzzleEntry {
	typedef Card* (*PuzzleFunc)(MerlinGame *game, Boltlib &boltlib, BltId resId);
	PuzzleFunc puzzle;
	uint16 resId;
	uint32 winMovie;
};

class MerlinGame : public FunhouseGame {
public:
	static const int kNumPotionMovies;

	enum PopupType {
		kHubPopup = 0,
		kPuzzlePopup = 1,
		kPotionPuzzlePopup = 2,
	};

	// From FunhouseGame
	virtual void init(OSystem *system, FunhouseEngine *engine, Audio::Mixer *mixer);
	virtual BoltRsp handleMsg(const BoltMsg &msg);
	virtual void win();
	
	void redraw();
	OSystem* getSystem();
	FunhouseEngine* getEngine();
	Graphics* getGraphics();
	bool isInMovie() const;
	void startMAMovie(uint32 name);
	void startPotionMovie(int num);
	BltId getPopupResId(PopupType type);
	bool isPuzzleSolved(int num) const;

	void branchScript(int idx, bool absolute = false);
	void branchReturn();
	void branchWin();
	void branchLoadProfile();

	bool doesProfileExist(int idx) const;
	int getProfile() const;
	void selectProfile(int idx);

	int getDifficulty(DifficultyCategory category) const;
	void setDifficulty(DifficultyCategory category, int level);

	bool getCheatMode() const;
	void setCheatMode(bool enable);

	static const int kNumFiles = 12;

private:
	friend class MovieCard;

	struct ScriptEntry;
	typedef void (MerlinGame::*ScriptFunc)(const ScriptEntry *entry);
	struct ScriptEntry {
		ScriptFunc func;
		int param;
		uint32 helpId;
		int branchTable[16];
	};

	void scriptPlotMovie(const ScriptEntry *entry);
	void scriptPostBumper(const ScriptEntry* entry);
	void scriptMenu(const ScriptEntry* entry);
	void scriptHub(const ScriptEntry* entry);
	void scriptFreeplay(const ScriptEntry* entry);
	template<class T>
	void scriptPuzzle(const ScriptEntry* entry);

	static const int kNumPopupTypes = 3;

	static const ScriptEntry kScript[];

	static const uint32 kPotionMovies[];

	void initCursor();
	void startMenu(BltId id);
	void startMovie(PfFile &pfFile, uint32 name);

	static void movieTrigger(void *param, uint16 triggerType);

	BoltRsp handleMsgInMovie(const BoltMsg &msg);
	BoltRsp handleMsgInCard(const BoltMsg &msg);

	OSystem *_system;
	FunhouseEngine *_engine;
	SaveManager _saveMan;

	Boltlib _boltlib;
	PfFile _maPf;
	PfFile _helpPf;
	PfFile _potionPf;
	PfFile _challdirPf;

	BltImage _cursorImage;

	Common::ScopedPtr<Card> _activeCard;
	Movie _movie;

	void setActiveCard(Card *card);
	void enterActiveCard(bool cursorActive);

	// Number of current file. -1 if no file is selected.
	int _fileNum = -1;
	bool _cheatMode = false;

	// Difficulty levels:
	// 0: beginner; 1: advanced; 2: expert; -1: not set
	int _difficulties[kNumDifficultyCategories];

	void runScript();
	
	static const int kInitialScriptCursor;
	static const int kNewGameScriptCursor;
	int _scriptCursor = 0;
	int _nextScriptCursor = 0;
	int _prevScriptCursor = 0;
	int _returnScriptCursor = 0;

	BltId _popupResIds[kNumPopupTypes];
};

} // End of namespace Funhouse

#endif
