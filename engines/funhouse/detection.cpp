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

#include "funhouse/bolt.h"
#include "funhouse/merlin/merlin.h"

#include "engines/advancedDetector.h"

namespace Funhouse {

// NOTE: Labyrinth of Crete is not yet supported or detected.
static const PlainGameDescriptor funhouseGames[] = {
	{ "merlin", "Merlin's Apprentice" },
	{ 0, 0 }
};

static const ADGameDescription gameDescriptions[] = {
	{
		"merlin",
		0,
		// FIXME: ScummVM will not detect BOLTLIB.BLT until you uncheck its
		// "Hidden" property with Explorer.
		// Fix ScummVM so it can detect hidden files!
		AD_ENTRY1("BOLTLIB.BLT", "58ef3e35e1f6369056272a30c67bb94d"),
		Common::EN_ANY,
		// Games were released for Win and Mac on the same CD-ROM. There are no
		// notable differences between the platforms.
		Common::kPlatformWindows,
		ADGF_UNSTABLE,
		GUIO0()
	},

	/*
	 * Notes about the CD-I games:
	 * The CD-I games have a screen resolution of 384x240. For Win/Mac, the
	 * resolution is 320x200 and graphics are cropped to fit.
	 * The CD-I games have a cross-fade effect that is absent in the CD-ROM
	 * version.
	 * The CD-I games have three redundant copies of the BOLTLIB file.
	 * The CD-I games have a different (but similar) movie format.
	 */

	AD_TABLE_END_MARKER
};

class FunhouseMetaEngine : public AdvancedMetaEngine {
public:
	FunhouseMetaEngine();

	virtual const char *getName() const;

	virtual const char *getOriginalCopyright() const;

protected:
	virtual bool createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const;
};

FunhouseMetaEngine::FunhouseMetaEngine() :
	AdvancedMetaEngine(Funhouse::gameDescriptions, sizeof(ADGameDescription), funhouseGames)
{ }

const char *FunhouseMetaEngine::getName() const {
	return "Funhouse";
}

const char *FunhouseMetaEngine::getOriginalCopyright() const {
	return "(C) 1994 Philips Interactive Media";
}

bool FunhouseMetaEngine::createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const
{
	*engine = nullptr;

	if (desc) {
		*engine = new FunhouseEngine(syst, desc);
	}

	return *engine != nullptr;
}

} // End of namespace Funhouse

#if PLUGIN_ENABLED_DYNAMIC(FUNHOUSE)
	REGISTER_PLUGIN_DYNAMIC(FUNHOUSE, PLUGIN_TYPE_ENGINE, Funhouse::FunhouseMetaEngine);
#else
	REGISTER_PLUGIN_STATIC(FUNHOUSE, PLUGIN_TYPE_ENGINE, Funhouse::FunhouseMetaEngine);
#endif
