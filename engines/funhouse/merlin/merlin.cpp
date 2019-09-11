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

#include "funhouse/merlin/merlin.h"

#include "common/events.h"
#include "common/system.h"
#include "gui/message.h"

#include "funhouse/bolt.h"
#include "funhouse/merlin/action_puzzle.h"
#include "funhouse/merlin/color_puzzle.h"
#include "funhouse/merlin/memory_puzzle.h"
#include "funhouse/merlin/potion_puzzle.h"
#include "funhouse/merlin/sliding_puzzle.h"
#include "funhouse/merlin/synch_puzzle.h"
#include "funhouse/merlin/tangram_puzzle.h"
#include "funhouse/merlin/word_puzzle.h"
#include "funhouse/merlin/hub.h"
#include "funhouse/merlin/main_menu.h"
#include "funhouse/merlin/file_menu.h"
#include "funhouse/merlin/difficulty_menu.h"

namespace Funhouse {

struct BltPopupCatalog {
    static const uint32 kType = kBltPopupCatalog;
    static const uint32 kSize = 0x22;
    void load(Common::Span<const byte> src, Boltlib &bltFile) {
        popupId[0] = BltId(src.getUint32BEAt(0x12));
        popupId[1] = BltId(src.getUint32BEAt(0x18));
        popupId[2] = BltId(src.getUint32BEAt(0x1E));
    }

    BltId popupId[3];
};

static const uint16 kPopupCatalogId = 0x0A04;

void MerlinGame::init(OSystem *system, FunhouseEngine *engine, Audio::Mixer *mixer) {
	_system = system;
	_engine = engine;
	_graphics = _engine->getGraphics();
	_mixer = mixer;
	_eventLoop = _engine;

	_boltlib.load("BOLTLIB.BLT");

	_maPf.load("MA.PF");
	_helpPf.load("HELP.PF");
	_potionPf.load("POTION.PF");
	_challdirPf.load("CHALLDIR.PF");

	_movie.setTriggerCallback(MerlinGame::movieTrigger, this);

    // Load popup catalog
    BltPopupCatalog popupCatalog;
    loadBltResource(popupCatalog, _boltlib, BltShortId(kPopupCatalogId));
    for (int i = 0; i < kNumPopupTypes; ++i) {
        BltU16Values popupIds;
        loadBltResourceArray(popupIds, _boltlib, popupCatalog.popupId[i]);
        _popupResIds[i] = BltShortId(popupIds[0].value);
    }

	// Load cursor
	initCursor();

	// Start sequence
	resetSequence();
}

BoltCmd MerlinGame::handleMsg(const BoltMsg &msg) {
	// Play movie over anything else
	if (_movie.isRunning()) {
		return handleMsgInMovie(msg);
	} else if (_currentCard) {
		return handleMsgInCard(msg);
	}

	assert(false); // Unreachable; there must be an active movie or card
	return BoltCmd::kDone;
}

OSystem* MerlinGame::getSystem() {
	return _system;
}

FunhouseEngine* MerlinGame::getEngine() {
	return _engine;
}

Graphics* MerlinGame::getGraphics() {
	return _graphics;
}

IBoltEventLoop* MerlinGame::getEventLoop() {
    return _eventLoop;
}

bool MerlinGame::isInMovie() const {
	return _movie.isRunning();
}

void MerlinGame::startMAMovie(uint32 name) {
	startMovie(_maPf, name);
}

void MerlinGame::startPotionMovie(int num) {
	if (num < 0 || num >= kNumPotionMovies) {
		warning("Tried to play invalid potion movie %d", num);
		return;
	}

	startMovie(_potionPf, kPotionMovies[num]);
}

BltId MerlinGame::getPopupResId(PopupType type) {
    return _popupResIds[type];
}

void MerlinGame::initCursor() {
	static const uint16 kCursorImageId = 0x9D00;
	static const byte kCursorPalette[3 * 2] = { 0, 0, 0, 255, 255, 255 };

	if (!_cursorImage) {
		_cursorImage.load(_boltlib, BltShortId(kCursorImageId));
	}

	::Graphics::Surface surface;
	surface.create(_cursorImage.getWidth(), _cursorImage.getHeight(),
		::Graphics::PixelFormat::createFormatCLUT8());
	_cursorImage.draw(surface, false);
	_system->setMouseCursor(surface.getPixels(),
		_cursorImage.getWidth(), _cursorImage.getHeight(),
		-_cursorImage.getOffset().x, -_cursorImage.getOffset().y, 0);
	_system->setCursorPalette(kCursorPalette, 0, 2);
	_system->showMouse(true);
	surface.free();
}

void MerlinGame::resetSequence() {
	_sequenceCursor = -1;
	advanceSequence();
}

void MerlinGame::advanceSequence() {
	// Advance sequence until movie or card becomes active
	_graphics->resetColorCycles(); // XXX: keeps cycles from sticking in wrong scenes; might break something?
	_graphics->setFade(1);
	do {
		++_sequenceCursor;
		if (_sequenceCursor >= kSequenceSize) {
			_sequenceCursor = 0;
		}
		enterSequenceEntry();
	} while (!_movie.isRunning() && !_currentCard);
}

// Call pointer to member function.
// See <https://isocpp.org/wiki/faq/pointers-to-members>
#define CALL_MEMBER_FN(object, fn) ((object).*(fn))

void MerlinGame::enterSequenceEntry() {
	_currentHub = nullptr;
	_currentPuzzle = nullptr;
	const Callback &callback = kSequence[_sequenceCursor];
	CALL_MEMBER_FN(*this, callback.func)(callback.param);
}

void MerlinGame::startMainMenu(BltId id) {
	_currentCard.reset();
	MainMenu* card = new MainMenu;
	card->init(this, _boltlib, id);
	setCurrentCard(card);
}

void MerlinGame::startFileMenu(BltId id) {
    _currentCard.reset();
    FileMenu* card = new FileMenu;
    card->init(this, _boltlib, id);
    setCurrentCard(card);
}

void MerlinGame::startDifficultyMenu(BltId id) {
    _currentCard.reset();
    DifficultyMenu* card = new DifficultyMenu;
    card->init(this, _boltlib, id);
    setCurrentCard(card);
}

void MerlinGame::exitOrReturn() {
    // TODO: Implement returning from hub to main menu, etc.
    enterSequenceEntry();
}

class GenericMenuCard : public Card {
public:
	void init(MerlinGame *game, Boltlib &boltlib, BltId id) {
		_scene.load(game->getEngine(), boltlib, id);
	}

	void enter() {
		_scene.enter();
	}

	BoltCmd handleMsg(const BoltMsg &msg) {
		if (msg.type == Scene::kClickButton) {
			warning("Unhandled button %d", msg.num);
			return Card::kEnd;
		}

		return _scene.handleMsg(msg);
	}

private:
	Scene _scene;
};

void MerlinGame::startMenu(BltId id) {
	_currentCard.reset();
	GenericMenuCard* menuCard = new GenericMenuCard;
	menuCard->init(this, _boltlib, id);
	setCurrentCard(menuCard);
}

void MerlinGame::startMovie(PfFile &pfFile, uint32 name) {
	// Color cycles do NOT stop when a movie starts.
	_movie.stop();
	_movie.start(_graphics, _mixer, _eventLoop, pfFile, name);
}

void MerlinGame::movieTrigger(void *param, uint16 triggerType) {
	MerlinGame *self = reinterpret_cast<MerlinGame*>(param);
	if (triggerType == 0x8002) {
		// Enter next card; used during win movies to transition back to hub card
		if (self->_currentCard) {
			self->enterCurrentCard(false);
		}
	}
	else {
		warning("unknown movie trigger 0x%.04X", (int)triggerType);
	}
}

BoltCmd MerlinGame::handleMsgInMovie(const BoltMsg &msg) {
	BoltCmd cmd = BoltCmd::kDone;
	if (msg.type == BoltMsg::kClick) {
		_movie.stop();
	} else {
		cmd = _movie.handleMsg(msg);
	}

	if (!_movie.isRunning()) {
		// If movie has stopped...
		_graphics->setFade(1);
		if (_currentCard) {
			enterCurrentCard(true);
		} else {
			advanceSequence();
		}
	}

	return cmd;
}

BoltCmd MerlinGame::handleMsgInCard(const BoltMsg &msg) {
	assert(_currentCard);

	BoltCmd cmd = _currentCard->handleMsg(msg);
	switch (cmd.type) {
	case Card::kEnd:
		advanceSequence();
		return BoltCmd::kDone;

    case Card::kReturn:
        exitOrReturn();
        return BoltCmd::kDone;

	case Card::kWin:
		win();
		return BoltCmd::kDone;

	case Card::kEnterPuzzle:
		if (cmd.num < 0 || cmd.num >= _currentHub->numPuzzles) {
			assert(false && "Tried to enter invalid puzzle number");
		}
		puzzle(&_currentHub->puzzles[cmd.num]);
		return BoltCmd::kDone;
	}

	return cmd;
}

void MerlinGame::redraw() {
    if (!isInMovie()) {
        assert(_currentCard);
        _currentCard->redraw();
    }
}

void MerlinGame::win() {
    if (_currentPuzzle) {
        _currentCard.reset();
	    startMovie(_challdirPf, _currentPuzzle->winMovie);
	    enterSequenceEntry(); // Return to hub
    } else {
        warning("Win procedure is not possible here");
    }
}

void MerlinGame::puzzle(const PuzzleEntry *entry) {
	_currentCard.reset();
	_currentPuzzle = entry;
	Card *card = _currentPuzzle->puzzle(this, _boltlib, BltShortId(_currentPuzzle->resId));
	setCurrentCard(card);
}

void MerlinGame::setCurrentCard(Card *card) {
	_currentCard.reset(card);
	if (!_movie.isRunning() && _currentCard) {
		// If there is no movie playing, enter new card now
		enterCurrentCard(true);
	}
}

void MerlinGame::enterCurrentCard(bool cursorActive) {
	assert(_currentCard);
	_graphics->resetColorCycles();
	_currentCard->enter();
	if (cursorActive) {
		BoltMsg hoverMsg(BoltMsg::kHover);
		hoverMsg.point = _system->getEventManager()->getMousePos();
		handleMsgInCard(hoverMsg);
	}
}

void MerlinGame::plotMovie(const void *param) {
	_currentCard.reset();
	uint32 name = *reinterpret_cast<const uint32*>(param);
	startMovie(_maPf, name);
}

void MerlinGame::mainMenu(const void *param) {
	static const uint16 kMainMenuId = 0x0118;
	startMainMenu(BltShortId(kMainMenuId));
}

void MerlinGame::fileMenu(const void *param) {
	static const uint16 kFileMenuId = 0x027A;
	startFileMenu(BltShortId(kFileMenuId));
}

void MerlinGame::difficultyMenu(const void *param) {
	static const uint16 kDifficultyMenuId = 0x006B;
	startDifficultyMenu(BltShortId(kDifficultyMenuId));
}

void MerlinGame::hub(const void *param) {
	_currentCard.reset();
	const HubEntry *entry = reinterpret_cast<const HubEntry*>(param);
	_currentHub = entry;
	HubCard *card = new HubCard;
	card->init(this, _boltlib, BltShortId(entry->hubId));
	setCurrentCard(card);
}

void MerlinGame::freeplayHub(const void *param) {
	_currentCard.reset();
	uint16 sceneId = *reinterpret_cast<const uint16*>(param);
	GenericMenuCard *card = new GenericMenuCard;
	card->init(this, _boltlib, BltShortId(sceneId));
	setCurrentCard(card);
}

void MerlinGame::potionPuzzle(const void *param) {
	_currentCard.reset();
	uint16 id = *reinterpret_cast<const uint16*>(param);
	PotionPuzzle *card = new PotionPuzzle;
	card->init(this, _eventLoop, _boltlib, BltShortId(id));
	setCurrentCard(card);
}

// Hardcoded values from MERLIN.EXE:
//
// Action puzzles:
//   SeedsDD    4921
//   LeavesDD   4D19
//   BubblesDD  5113
//   SnowflakDD 551C
//   GemsDD     5918
//   DemonsDD   5D17
//
// Word puzzles:
//   GraveDD  61E3
//   ParchDD  69E1
//   TabletDD 65E1
//
// Tangram puzzles:
//   MirrorDD  7115
//   PlaqueDD  6D15
//   OctagonDD 7515
//   TileDD    7915
//
// Sliding puzzles:
//   RavenDD  353F
//   LeafDD   313F
//   SnakeDD  4140
//   SkeltnDD 3D3F
//   SpiderDD 453F
//   QuartzDD 393F
//
// Synchronization puzzles:
//   PlanetDD 7D12
//   DoorDD   8114
//   SphereDD 8512
//
// Color puzzles:
//   WindowDD 8C13
//   StarDD   9014
//
// Potion puzzles:
//   ForestDD 940C
//   LabratDD 980C
//   CavernDD 9C0E
//
// Memory puzzles:
//   PondDD   865E
//   FlasksDD 8797
//   StalacDD 887B
//
// Potion movies:
//   'ELEC', 'EXPL', 'FLAM', 'FLSH', 'MIST', 'OOZE', 'SHMR',
//   'SWRL', 'WIND', 'BOIL', 'BUBL', 'BSPK', 'FBRS', 'FCLD',
//   'FFLS', 'FSWR', 'LAVA', 'LFIR', 'LSMK', 'SBLS', 'SCLM',
//   'SFLS', 'SPRE', 'WSTM', 'WSWL', 'BUGS', 'CRYS', 'DNCR',
//   'FISH', 'GLAC', 'GOLM', 'EYEB', 'MOLE', 'MOTH', 'MUDB',
//   'ROCK', 'SHTR', 'SLUG', 'SNAK', 'SPKB', 'SPKM', 'SPDR',
//   'SQID', 'CLOD', 'SWIR', 'VOLC', 'WORM',
//
// TODO: there are more: cursor, menus, etc.

template<class T>
static Card* makePuzzle(MerlinGame *game, Boltlib &boltlib, BltId resId) {
	T *card = new T;
	card->init(game, boltlib, resId);
	return card;
}

static const PuzzleEntry::PuzzleFunc makeActionPuzzle = makePuzzle<ActionPuzzle>;
static const PuzzleEntry::PuzzleFunc makeWordPuzzle = makePuzzle<WordPuzzle>;
static const PuzzleEntry::PuzzleFunc makeSlidingPuzzle = makePuzzle<SlidingPuzzle>;
static const PuzzleEntry::PuzzleFunc makeMemoryPuzzle = makePuzzle <MemoryPuzzle>;
static const PuzzleEntry::PuzzleFunc makeTangramPuzzle = makePuzzle<TangramPuzzle>;
static const PuzzleEntry::PuzzleFunc makeSynchPuzzle = makePuzzle<SynchPuzzle>;
static const PuzzleEntry::PuzzleFunc makeColorPuzzle = makePuzzle<ColorPuzzle>;

const HubEntry MerlinGame::kStage1 = { 0x0C0B, 6, MerlinGame::kStage1Puzzles };
const PuzzleEntry MerlinGame::kStage1Puzzles[6] = {
	{ makeActionPuzzle,  0x4921, MKTAG('S', 'E', 'E', 'D') }, // seeds
	{ makeWordPuzzle,    0x61E3, MKTAG('G', 'R', 'A', 'V') }, // grave
	{ makeSlidingPuzzle, 0x313F, MKTAG('O', 'A', 'K', 'L') }, // oak leaf
	{ makeMemoryPuzzle,  0x865E, MKTAG('P', 'O', 'N', 'D') }, // pond
	{ makeActionPuzzle,  0x4D19, MKTAG('L', 'E', 'A', 'V') }, // leaves
	{ makeSlidingPuzzle, 0x353F, MKTAG('R', 'A', 'V', 'N') }, // raven
};

const HubEntry MerlinGame::kStage2 = { 0x0D34, 9, MerlinGame::kStage2Puzzles };
const PuzzleEntry MerlinGame::kStage2Puzzles[9] = {
	{ makeSlidingPuzzle, 0x4140, MKTAG('R', 'T', 'T', 'L') }, // rattlesnake
	{ makeTangramPuzzle, 0x6D15, MKTAG('P', 'L', 'A', 'Q') }, // plaque
	{ makeActionPuzzle,  0x551C, MKTAG('S', 'N', 'O', 'W') }, // snow
	{ makeSynchPuzzle,   0x7D12, MKTAG('P', 'L', 'N', 'T') }, // planets
	{ makeWordPuzzle,    0x69E1, MKTAG('P', 'R', 'C', 'H') }, // parchment
	{ makeActionPuzzle,  0x5113, MKTAG('B', 'B', 'L', 'E') }, // bubbles
	{ makeSlidingPuzzle, 0x3D3F, MKTAG('S', 'K', 'L', 'T') }, // skeleton
	{ makeMemoryPuzzle,  0x8797, MKTAG('F', 'L', 'S', 'K') }, // flasks
	{ makeTangramPuzzle, 0x7115, MKTAG('M', 'I', 'R', 'R') }, // mirror
};

const HubEntry MerlinGame::kStage3 = { 0x0E4F, 12, MerlinGame::kStage3Puzzles };
const PuzzleEntry MerlinGame::kStage3Puzzles[12] = {
	{ makeColorPuzzle,   0x8C13, MKTAG('W', 'N', 'D', 'W') }, // window
	{ makeTangramPuzzle, 0x7515, MKTAG('O', 'C', 'T', 'A') }, // octagon
	{ makeSynchPuzzle,   0x8512, MKTAG('S', 'P', 'R', 'T') }, // spirits
	{ makeColorPuzzle,   0x9014, MKTAG('S', 'T', 'A', 'R') }, // star
	{ makeSynchPuzzle,   0x8114, MKTAG('D', 'O', 'O', 'R') }, // door
	{ makeActionPuzzle,  0x5918, MKTAG('G', 'E', 'M', 'S') }, // gems
	{ makeSlidingPuzzle, 0x393F, MKTAG('C', 'S', 'T', 'L') }, // crystal
	{ makeActionPuzzle,  0x5D17, MKTAG('D', 'E', 'M', 'N') }, // demons
	{ makeTangramPuzzle, 0x7915, MKTAG('T', 'I', 'L', 'E') }, // tile
	{ makeSlidingPuzzle, 0x453F, MKTAG('S', 'P', 'I', 'D') }, // spider
	{ makeWordPuzzle,    0x65E1, MKTAG('T', 'B', 'L', 'T') }, // tablet
	{ makeMemoryPuzzle,  0x887B, MKTAG('S', 'T', 'L', 'C') }, // stalactites & stalagmites
};

static const uint32 kPlotMovieBMPR = MKTAG('B', 'M', 'P', 'R');
static const uint32 kPlotMovieINTR = MKTAG('I', 'N', 'T', 'R');
static const uint32 kPlotMoviePLOG = MKTAG('P', 'L', 'O', 'G');
static const uint32 kPlotMovieLABT = MKTAG('L', 'A', 'B', 'T');
static const uint32 kPlotMovieCAV1 = MKTAG('C', 'A', 'V', '1');
static const uint32 kPlotMovieFNLE = MKTAG('F', 'N', 'L', 'E');

static const uint16 kFreeplayScenes = 0x0600; // TODO: 0600 contains ID's for freeplay hubs
static const uint16 kFreeplayScene1 = 0x0337; // so stop hardcoding these
static const uint16 kFreeplayScene2 = 0x0446;
static const uint16 kFreeplayScene3 = 0x0555;

static const uint16 kPotionPuzzle1 = 0x940C;
static const uint16 kPotionPuzzle2 = 0x980C;
static const uint16 kPotionPuzzle3 = 0x9C0E;

const MerlinGame::Callback
MerlinGame::kSequence[] = {
	// Pre-game menus
	{ &MerlinGame::plotMovie, &kPlotMovieBMPR },
	{ &MerlinGame::plotMovie, &kPlotMovieINTR },
	{ &MerlinGame::mainMenu, nullptr },
	{ &MerlinGame::fileMenu, nullptr },
	{ &MerlinGame::difficultyMenu, nullptr },

	// Stage 1: Forest
	{ &MerlinGame::plotMovie, &kPlotMoviePLOG },
	{ &MerlinGame::hub, &MerlinGame::kStage1 },

	// Stage 2: Laboratory
	{ &MerlinGame::plotMovie, &kPlotMovieLABT },
	{ &MerlinGame::hub, &MerlinGame::kStage2 },

	// Stage 3: Cave
	{ &MerlinGame::plotMovie, &kPlotMovieCAV1 },
	{ &MerlinGame::hub, &MerlinGame::kStage3 },

	// Finale movie is hidden until the game is fully implemented. 
	//{ &MerlinGame::plotMovie, &kPlotMovieFNLE },

	{ &MerlinGame::freeplayHub, &kFreeplayScene1 },
	{ &MerlinGame::potionPuzzle, &kPotionPuzzle1 },
	{ &MerlinGame::freeplayHub, &kFreeplayScene2 },
	{ &MerlinGame::potionPuzzle, &kPotionPuzzle2 },
	{ &MerlinGame::freeplayHub, &kFreeplayScene3 },
	{ &MerlinGame::potionPuzzle, &kPotionPuzzle3 },
};

const int MerlinGame::kSequenceSize =
	sizeof(MerlinGame::kSequence) /
	sizeof(MerlinGame::Callback);

const uint32 MerlinGame::kPotionMovies[] = {
	MKTAG('E','L','E','C'), MKTAG('E','X','P','L'), MKTAG('F','L','A','M'),
	MKTAG('F','L','S','H'), MKTAG('M','I','S','T'), MKTAG('O','O','Z','E'),
	MKTAG('S','H','M','R'), MKTAG('S','W','R','L'), MKTAG('W','I','N','D'),
	MKTAG('B','O','I','L'), MKTAG('B','U','B','L'), MKTAG('B','S','P','K'),
	MKTAG('F','B','R','S'), MKTAG('F','C','L','D'), MKTAG('F','F','L','S'),
	MKTAG('F','S','W','R'), MKTAG('L','A','V','A'), MKTAG('L','F','I','R'),
	MKTAG('L','S','M','K'), MKTAG('S','B','L','S'), MKTAG('S','C','L','M'),
	MKTAG('S','F','L','S'), MKTAG('S','P','R','E'), MKTAG('W','S','T','M'),
	MKTAG('W','S','W','L'), MKTAG('B','U','G','S'), MKTAG('C','R','Y','S'),
	MKTAG('D','N','C','R'), MKTAG('F','I','S','H'), MKTAG('G','L','A','C'),
	MKTAG('G','O','L','M'), MKTAG('E','Y','E','B'), MKTAG('M','O','L','E'),
	MKTAG('M','O','T','H'), MKTAG('M','U','D','B'), MKTAG('R','O','C','K'),
	MKTAG('S','H','T','R'), MKTAG('S','L','U','G'), MKTAG('S','N','A','K'),
	MKTAG('S','P','K','B'), MKTAG('S','P','K','M'), MKTAG('S','P','D','R'),
	MKTAG('S','Q','I','D'), MKTAG('C','L','O','D'), MKTAG('S','W','I','R'),
	MKTAG('V','O','L','C'), MKTAG('W','O','R','M'),
};

const int MerlinGame::kNumPotionMovies =
	sizeof(MerlinGame::kPotionMovies) / sizeof(uint32);

} // End of namespace Funhouse
