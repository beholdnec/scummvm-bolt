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
    IBoltEventLoop* getEventLoop();
	bool isInMovie() const;
	void startMAMovie(uint32 name);
	void startPotionMovie(int num);
	int getFile() const;
	void setFile(int num);
    BltId getPopupResId(PopupType type);
	bool isPuzzleSolved(int num) const;

    int getDifficulty(DifficultyCategory category) const;
    void setDifficulty(DifficultyCategory category, int level);

    bool getCheatMode() const;
    void setCheatMode(bool enable);

	static const int kNumFiles = 12;

private:
    typedef void (MerlinGame::*CallbackFunc)(const void *param);
    struct Callback {
        CallbackFunc func;
        const void *param;
    };

	struct ScriptEntry;
	typedef int (MerlinGame::*ScriptFunc)(const ScriptEntry *entry);
	struct ScriptEntry {
		ScriptFunc func;
		int param;
		uint32 helpId;
	};

	int scriptPlotMovie(const ScriptEntry *entry);
	int scriptPostBumper(const ScriptEntry* entry);
	int scriptMenu(const ScriptEntry* entry);
	int scriptHub(const ScriptEntry* entry);
	int scriptFreeplay(const ScriptEntry* entry);

	int scriptActionPuzzle(const ScriptEntry* entry);
	int scriptColorPuzzle(const ScriptEntry* entry);
	int scriptMemoryPuzzle(const ScriptEntry* entry);
	int scriptPotionPuzzle(const ScriptEntry* entry);
	int scriptSlidingPuzzle(const ScriptEntry* entry);
	int scriptSynchPuzzle(const ScriptEntry* entry);
	int scriptTangramPuzzle(const ScriptEntry* entry);
	int scriptWordPuzzle(const ScriptEntry* entry);

    static const int kNumPopupTypes = 3;

    static const HubEntry kStage1;
    static const PuzzleEntry kStage1Puzzles[6];
    static const HubEntry kStage2;
    static const PuzzleEntry kStage2Puzzles[9];
    static const HubEntry kStage3;
    static const PuzzleEntry kStage3Puzzles[12];
	static const HubEntry* const kHubEntries[3];

    static const Callback kSequence[];
    static const int kSequenceSize;

	// TODO: remove kSequence and use kScript instead
	static const ScriptEntry kScript[];
	static const int kScriptLength;

    static const uint32 kPotionMovies[];

	void initCursor();
	void resetSequence();
	void advanceSequence();
	void enterSequenceEntry();
	void startMainMenu(BltId id);
    void startFileMenu(BltId id);
    void startDifficultyMenu(BltId id);
	void startMenu(BltId id);
	void startMovie(PfFile &pfFile, uint32 name);
    void exitOrReturn();

	static void movieTrigger(void *param, uint16 triggerType);

	BoltRsp handleMsgInMovie(const BoltMsg &msg);
	BoltRsp handleMsgInCard(const BoltMsg &msg);
	void puzzle(int num, const PuzzleEntry *entry);

	OSystem *_system;
	FunhouseEngine *_engine;
	Graphics *_graphics;
	Audio::Mixer *_mixer;
	IBoltEventLoop *_eventLoop;

	Boltlib _boltlib;
	PfFile _maPf;
	PfFile _helpPf;
	PfFile _potionPf;
	PfFile _challdirPf;

	BltImage _cursorImage;

	Common::ScopedPtr<Card> _currentCard;
	Movie _movie;

	void setCurrentCard(Card *card);
	void enterCurrentCard(bool cursorActive);

	// Number of current file. -1 if no file is selected.
	int _fileNum;
    bool _cheatMode;

	// Difficulty levels:
	// 0: beginner; 1: advanced; 2: expert; -1: not set
    int _difficulties[kNumDifficultyCategories];

	int _sequenceCursor;
	const HubEntry *_currentHub;
	const PuzzleEntry *_currentPuzzle;

	int _currentHubNum;
	int _currentPuzzleNum;
	Common::Array<bool> _puzzlesSolved;

    BltId _popupResIds[kNumPopupTypes];

	void plotMovie(const void *param);
	void mainMenu(const void *param);
	void fileMenu(const void *param);
	void difficultyMenu(const void *param);
	void hub(const void *param);
	void freeplayHub(const void *param);
	void potionPuzzle(const void *param);
};

} // End of namespace Funhouse

#endif
