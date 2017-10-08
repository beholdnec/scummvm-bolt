﻿#!/usr/bin/env python3

# ScummVM - Graphic Adventure Engine
#
# ScummVM is the legal property of its developers, whose names
# are too numerous to list here. Please refer to the COPYRIGHT
# file distributed with this source distribution.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#

# Requires PySide and construct library

"""BLT Viewer main program."""

import io
import sys

from PySide import QtGui
from PySide.QtCore import Qt

from construct import *

from internal.bltfile import BltFile
from internal.rl7 import decode_rl7

# Generate _TEST_PALETTE
_TEST_PALETTE = [0] * 256 # holds 32-bit numbers formatted with bytes A, R, G, B
for r in range(0, 4):
    for g in range(0, 8):
        for b in range(0, 4):
            i = 8 * 4 * r + 4 * g + b
            _TEST_PALETTE[i] = 0xFF000000 | ((r * 255 // 3) << 16) | \
                ((g * 255 // 7) << 8) | (b * 255 // 3)
# Repeat for both planes
_TEST_PALETTE[128:256] = _TEST_PALETTE[0:128]

_IMAGE_COMPRESSION_NAMES = {
    0: "CLUT7",
    1: "RL7",
    }

_ImageHeaderStruct = "_ImageHeaderStruct" / Struct(
    "compression" / Int8ub,
    "unk_1" / Int8ub,
    "unk_2" / Int16ub,
    "unk_4" / Int16ub,
    "offset_x" / Int16sb,
    "offset_y" / Int16sb,
    "width" / Int16ub,
    "height" / Int16ub,
    )

class BltImageWidget(QtGui.QLabel):
    def __init__(self, data, palette):
        header = _ImageHeaderStruct.parse(data)
        image_data = data[0x18:]

        if header.compression == 0:
            # CLUT7
            super().__init__()
            image = QtGui.QImage(image_data, header.width, header.height, header.width, QtGui.QImage.Format_Indexed8)
            image.setColorTable(palette)
            self.setPixmap(QtGui.QPixmap.fromImage(image).scaled(header.width * 2, header.height * 2))
        elif header.compression == 1:
            # RL7
            super().__init__()
            decoded_image = bytearray(header.width * header.height)
            decode_rl7(decoded_image, image_data, header.width, header.height)
            image = QtGui.QImage(decoded_image, header.width, header.height, header.width, QtGui.QImage.Format_Indexed8)
            image.setColorTable(palette)
            self.setPixmap(QtGui.QPixmap.fromImage(image).scaled(header.width * 2, header.height * 2))
        else:
            super().__init__("Unsupported compression type {}".format(compression))

class MyTableWidget(QtGui.QTableWidget):
    def __init__(self, *col_labels):
        super().__init__()

        self.setColumnCount(len(col_labels))
        self.setHorizontalHeaderLabels(col_labels)

    def add_row(self, row_label, *values):
        row_num = self.rowCount()
        self.setRowCount(row_num + 1)

        headerItem = QtGui.QTableWidgetItem(row_label)
        headerItem.setFlags(Qt.NoItemFlags)
        self.setVerticalHeaderItem(row_num, headerItem)

        for i in range(0, len(values)):
            valueItem = QtGui.QTableWidgetItem(values[i])
            valueItem.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
            self.setItem(row_num, i, valueItem)


_BOLTLIB_FILE_SIZE_PLATFORMS = {
    11724174: ("Labyrinth of Crete PC/Mac", "PC"),
    10452486: ("Merlin's Apprentice PC/Mac", "PC"),
    8007558: ("Merlin's Apprentice CD-I", "CDI"),
    }

_RES_TYPE_HANDLERS = {
    "PC": {},
    "CDI": {},
    }

def _pc_res_handler(type):
    def decorate(cls):
        _RES_TYPE_HANDLERS["PC"][type] = cls
        return cls
    return decorate

def _cdi_res_handler(type):
    def decorate(cls):
        _RES_TYPE_HANDLERS["CDI"][type] = cls
        return cls
    return decorate

@_pc_res_handler(1)
@_cdi_res_handler(1)
class _Values8BitHandler:
    name = "8-bit Values"

    def open(res, container, app):
        newWidget = MyTableWidget("Value")

        for i in range(0, len(res.data)):
            val = res.data[i]
            newWidget.add_row("{}".format(i), "0x{:02X}".format(val))

        container.addWidget(newWidget)

@_pc_res_handler(3)
class _Values16BitHandler:
    name = "16-bit Values"

    def open(res, container, app):
        newWidget = MyTableWidget("Value")

        parsed = GreedyRange(UBInt16("value")).parse(res.data)
        for i in range(0, len(parsed)):
            newWidget.add_row("{}".format(i), "0x{:04X}".format(parsed[i]))

        container.addWidget(newWidget)

@_pc_res_handler(6)
@_cdi_res_handler(6)
class _ResourceListHandler:
    name = "Resource List"

    def open(res, container, app):
        newWidget = MyTableWidget("ID")

        parsed = GreedyRange(UBInt32("value")).parse(res.data)
        for i in range(0, len(parsed)):
            newWidget.add_row("{}".format(i), "0x{:08X}".format(parsed[i]))

        container.addWidget(newWidget)

@_pc_res_handler(7)
class _PcSoundHandler:
    name = "Sound"

@_pc_res_handler(8)
@_cdi_res_handler(8)
class _ImageHandler:
    name = "Image"

    def open(res, container, app):
        header = _ImageHeaderStruct.parse(res.data)

        newLayout = QtGui.QVBoxLayout()

        info_table = MyTableWidget("Value")
        compression_name = _IMAGE_COMPRESSION_NAMES.get(header.compression, "Unknown")
        info_table.add_row("Compression", "{} ({})".format(compression_name, header.compression))
        info_table.add_row("Unk @1", "0x{:02X}".format(header.unk_1))
        info_table.add_row("Unk @2", "0x{:04X}".format(header.unk_2))
        info_table.add_row("Unk @4", "0x{:04X}".format(header.unk_4))
        info_table.add_row("Offset", "({}, {})".format(header.offset_x, header.offset_y))
        info_table.add_row("Width", "{}".format(header.width))
        info_table.add_row("Height", "{}".format(header.height))
        newLayout.addWidget(info_table)

        newLayout.addWidget(QtGui.QLabel("Tip: If colors look wrong, load a palette."))

        newLayout.addWidget(BltImageWidget(res.data, app.cur_palette))

        newWidget = QtGui.QWidget()
        newWidget.setLayout(newLayout)
        container.addWidget(newWidget)

_ColorStruct = "_ColorStruct" / Struct(
    "r" / Int8ub,
    "g" / Int8ub,
    "b" / Int8ub,
    )

class _PaletteWidget(QtGui.QWidget):
    """A grid of colors."""

    _NUM_COLUMNS = 16

    def __init__(self, colors):
        """Initialize widget with an iterable of _ColorStruct's."""

        super().__init__()
        layout = QtGui.QGridLayout()
        self.setLayout(layout)
        for i in range(0, len(colors)):
            row = i // self._NUM_COLUMNS
            col = i % self._NUM_COLUMNS
            item = QtGui.QWidget()
            item.setAutoFillBackground(True)
            item_palette = QtGui.QPalette()
            qcolor = QtGui.QColor(colors[i].r, colors[i].g, colors[i].b)
            item_palette.setColor(QtGui.QPalette.Background, qcolor)
            item.setPalette(item_palette)
            layout.addWidget(item, row, col)

@_pc_res_handler(10)
@_cdi_res_handler(10)
class _PaletteHandler:
    name = "Palette"

    def open(res, container, app):

        # TODO: parse 6 bytes of header info (plane, number of colors, etc.)
        colors = GreedyRange(_ColorStruct).parse(res.data[6:])

        palette_widget = _PaletteWidget(colors)

        # TODO: handle planes
        app.cur_palette = [0] * 256
        for i in range(0, len(colors)):
            app.cur_palette[i] = 0xFF000000 | (colors[i].r << 16) | (colors[i].g << 8) | colors[i].b

        new_layout = QtGui.QVBoxLayout()
        new_layout.addWidget(palette_widget)
        new_layout.addWidget(QtGui.QLabel("Palette loaded."))

        new_widget = QtGui.QWidget()
        new_widget.setLayout(new_layout)
        container.addWidget(new_widget)

_ColorCyclesStruct = "_ColorCyclesStruct" / Struct(
    Array(4, "num_slots" / Int16ub),
    Array(4, "slot_ids" / Int32ub),
    )

@_pc_res_handler(11)
@_cdi_res_handler(11)
class _ColorCyclesHandler:
    name = "Color Cycles"

    def open(res, container, app):
        new_widget = MyTableWidget("# Slots", "Slots ID")

        parsed = _ColorCyclesStruct.parse(res.data)
        for i in range(0, 4):
            new_widget.add_row("{}".format(i),
                "{}".format(parsed.num_slots[i]),
                "0x{:08X}".format(parsed.slot_ids[i])
                )

        container.addWidget(new_widget)

_ColorCycleSlotStruct = "_ColorCycleSlotStruct" / Struct(
    "start" / Int16ub,
    "end" / Int16ub,
    "unk_4" / Int16ub,
    )

@_pc_res_handler(12)
@_cdi_res_handler(12)
class _ColorCycleSlotHandler:
    name = "Color Cycle Slot"

    def open(res, container, app):
        new_widget = MyTableWidget("Value")

        parsed = _ColorCycleSlotStruct.parse(res.data)
        new_widget.add_row("Start", "{}".format(parsed.start))
        new_widget.add_row("End", "{}".format(parsed.end))
        new_widget.add_row("Unk @4", "0x{:04X}".format(parsed.unk_4))

        container.addWidget(new_widget)

@_cdi_res_handler(19)
class _CdiSoundHandler:
    name = "Sound"

_PlaneStruct = "_PlaneStruct" / Struct(
    "image_id" / Int32ub,
    "palette_id" / Int32ub,
    "hotspots_id" / Int32ub,
    "unk_c" / Int32ub,
    )

@_pc_res_handler(26)
@_cdi_res_handler(27)
class _PlaneHandler:
    name = "Plane"

    def open(res, container, app):
        newWidget = MyTableWidget("Value")

        parsed = _PlaneStruct.parse(res.data)
        newWidget.add_row("Image ID", "0x{:08X}".format(parsed.image_id))
        newWidget.add_row("Palette ID", "0x{:08X}".format(parsed.palette_id))
        newWidget.add_row("Hotspots ID", "0x{:08X}".format(parsed.hotspots_id))
        newWidget.add_row("Unk @C", "0x{:08X}".format(parsed.unk_c))

        container.addWidget(newWidget)

_SpriteStruct = "_SpriteStruct" / Struct(
    "x" / Int16sb,
    "y" / Int16sb,
    "image_id" / Int32ub,
    )

# Ex: 370E
@_pc_res_handler(27)
@_cdi_res_handler(28)
class _SpritesHandler:
    name = "Sprites"

    def open(res, container, app):
        newWidget = MyTableWidget("Position", "Image ID")

        parsed = GreedyRange(_SpriteStruct).parse(res.data)
        for i in range(0, len(parsed)):
            newWidget.add_row("{}".format(i),
                "({}, {})".format(parsed[i].x, parsed[i].y),
                "0x{:08X}".format(parsed[i].image_id))


        container.addWidget(newWidget)

@_pc_res_handler(28)
@_cdi_res_handler(29)
class _ColorsHandler:
    name = "Colors"

    def open(res, container, app):
        colors = GreedyRange(_ColorStruct).parse(res.data)
        container.addWidget(_PaletteWidget(colors))

_PaletteModStruct = "_ButtonPaletteModStruct" / Struct(
    "index" / Int8ub,
    "count" / Int8ub,
    "colors_id" / Int32ub,
    )

@_pc_res_handler(29)
@_cdi_res_handler(30)
class _PaletteModHandler:
    name = "Palette Mod"

    def open(res, container, app):
        newWidget = MyTableWidget("Index", "Count", "Colors ID")

        parsed = GreedyRange(_PaletteModStruct).parse(res.data)
        for i in range(0, len(parsed)):
            newWidget.add_row("{}".format(i),
                "{}".format(parsed[i].index),
                "{}".format(parsed[i].count),
                "0x{:08X}".format(parsed[i].colors_id))

        container.addWidget(newWidget)

_BUTTON_GRAPHICS_TYPE_NAMES = {
    1: "Palette Mods",
    2: "Sprites",
    }

_ButtonGraphicsStruct = "_ButtonGraphicsStruct" / Struct(
    "type" / Int16ub,
    "unk_2" / Int32ub,
    "hovered_id" / Int32ub,
    "idle_id" / Int32ub,
    )

# Ex: 69B5
@_pc_res_handler(30)
@_cdi_res_handler(31)
class _ButtonGraphicsHandler:
    name = "Button Graphics"

    def open(res, container, app):
        newWidget = MyTableWidget("Type", "Unk @2", "Hovered", "Idle")

        parsed = GreedyRange(_ButtonGraphicsStruct).parse(res.data)
        for i in range(0, len(parsed)):
            type_name = _BUTTON_GRAPHICS_TYPE_NAMES.get(parsed[i].type, "Unknown")
            newWidget.add_row("{}".format(i),
                "{} ({})".format(type_name, parsed[i].type),
                "0x{:08X}".format(parsed[i].unk_2),
                "0x{:08X}".format(parsed[i].hovered_id),
                "0x{:08X}".format(parsed[i].idle_id))

        container.addWidget(newWidget)

_BUTTON_TYPE_NAMES = {
    1: "Rectangle",
    3: "Hotspot Query",
    }

_ButtonStruct = "_ButtonStruct" / Struct(
    "type" / Int16ub,
    "left" / Int16ub,
    "right" / Int16ub,
    "top" / Int16ub,
    "bottom" / Int16ub,
    "plane" / Int16ub,
    "num_graphics" / Int16ub,
    "unk_e" / Int16ub,
    "graphics_id" / Int32ub,
    )

# Ex: 312D
@_pc_res_handler(31)
@_cdi_res_handler(32)
class _ButtonsHandler:
    name = "Buttons"

    def open(res, container, app):
        newWidget = MyTableWidget("Type", "(L, R, T, B)", "Plane", "# Graphics",
            "Unk @E", "Graphics ID")

        parsed = GreedyRange(_ButtonStruct).parse(res.data)
        for i in range(0, len(parsed)):
            type_name = _BUTTON_TYPE_NAMES.get(parsed[i].type, "Unknown")
            newWidget.add_row("{}".format(i),
                "{} ({})".format(type_name, parsed[i].type),
                "({}, {}, {}, {})".format(parsed[i].left, parsed[i].right, parsed[i].top, parsed[i].bottom),
                "{}".format(parsed[i].plane),
                "{}".format(parsed[i].num_graphics),
                "{}".format(parsed[i].unk_e),
                "0x{:08X}".format(parsed[i].graphics_id))

        container.addWidget(newWidget)

_PcSceneStruct = "_PcSceneStruct" / Struct(
    "fore_plane_id" / Int32ub,
    "back_plane_id" / Int32ub,
    "num_sprites" / Int8ub,
    "unk_9" / Int8ub,
    "sprites_id" / Int32ub,
    "unk_e" / Int32ub,
    "unk_12" / Int32ub,
    "color_cycles_id" / Int32ub,
    "num_buttons" / Int16ub,
    "buttons_id" / Int32ub,
    "origin_x" / Int16sb,
    "origin_y" / Int16sb,
    )

# Ex: 3A0B
@_pc_res_handler(32)
class _SceneHandler:
    name = "Scene"

    def open(res, container, app):
        newWidget = MyTableWidget("Value")

        parsed = _PcSceneStruct.parse(res.data)
        newWidget.add_row("Fore Plane ID", "0x{:08X}".format(parsed.fore_plane_id))
        newWidget.add_row("Back Plane ID", "0x{:08X}".format(parsed.back_plane_id))
        newWidget.add_row("# Sprites", "0x{:02X}".format(parsed.num_sprites))
        newWidget.add_row("Unk @9", "0x{:02X}".format(parsed.unk_9))
        newWidget.add_row("Sprites ID", "0x{:08X}".format(parsed.sprites_id))
        newWidget.add_row("Unk @Eh", "0x{:08X}".format(parsed.unk_e))
        newWidget.add_row("Unk @12h", "0x{:08X}".format(parsed.unk_12))
        newWidget.add_row("Color Cycles ID", "0x{:08X}".format(parsed.color_cycles_id))
        newWidget.add_row("# Buttons", "{}".format(parsed.num_buttons))
        newWidget.add_row("Buttons ID", "0x{:08X}".format(parsed.buttons_id))
        newWidget.add_row("Origin", "({}, {})".format(parsed.origin_x, parsed.origin_y))

        container.addWidget(newWidget)

_CdiSceneStruct = "_CdiSceneStruct" / Struct(
    "fore_plane_id" / Int32ub,
    "back_plane_id" / Int32ub,
    "num_sprites" / Int8ub,
    "unk_9" / Int8ub,
    "sprites_id" / Int32ub,
    "unk_e" / Int32ub,
    "unk_12" / Int32ub,
    "color_cycles_id" / Int32ub,
    "num_buttons" / Int16ub,
    "buttons_id" / Int32ub,
    )
@_cdi_res_handler(33)
class _CdiSceneHandler:
    name = "Scene"

    def open(res, container, app):
        newWidget = MyTableWidget("Value")

        parsed = _CdiSceneStruct.parse(res.data)
        newWidget.add_row("Fore Plane ID", "0x{:08X}".format(parsed.fore_plane_id))
        newWidget.add_row("Back Plane ID", "0x{:08X}".format(parsed.back_plane_id))
        newWidget.add_row("# Sprites", "0x{:02X}".format(parsed.num_sprites))
        newWidget.add_row("Unk @9", "0x{:02X}".format(parsed.unk_9))
        newWidget.add_row("Sprites ID", "0x{:08X}".format(parsed.sprites_id))
        newWidget.add_row("Unk @Eh", "0x{:08X}".format(parsed.unk_e))
        newWidget.add_row("Unk @12h", "0x{:08X}".format(parsed.unk_12))
        newWidget.add_row("Color Cycles ID", "0x{:08X}".format(parsed.color_cycles_id))
        newWidget.add_row("# Buttons", "{}".format(parsed.num_buttons))
        newWidget.add_row("Buttons ID", "0x{:08X}".format(parsed.buttons_id))

        container.addWidget(newWidget)

_MainMenuStruct = "_MainMenuStruct" / Struct(
    "scene_id" / Int32ub,
    "colorbars_id" / Int32ub,
    "colorbars_palette_id" / Int32ub,
    )

# Ex: 0118
@_pc_res_handler(33)
class _MainMenuHandler:
    name = "Main Menu"

    def open(res, container, app):
        newWidget = MyTableWidget("Value")

        parsed = _MainMenuStruct.parse(res.data)
        newWidget.add_row("Scene ID", "0x{:08X}".format(parsed.scene_id))
        newWidget.add_row("Color Bars ID", "0x{:08X}".format(parsed.colorbars_id))
        newWidget.add_row("Color Bars Palette ID", "0x{:08X}".format(parsed.colorbars_palette_id))

        container.addWidget(newWidget)

_FileMenuStruct = "_FileMenuStruct" / Struct(
    "scene_id" / Int32ub,
    "select_game_piece_id" / Int32ub,
    "set_new_id" / Int32ub,
    "new_id" / Int32ub,
    "solved_id" / Int32ub,
    "one_more_id" / Int32ub,
    "x_more_id" / Int32ub,
    "xx_more_id" / Int32ub,
    "unk_20" / Int32ub,
    "unk_24" / Int32ub,
    Array(10, "tens_digit_id" / Int32ub), # 28
    Array(10, "ones_digit_id" / Int32ub), # 50
    Array(10, "unk_digit_id" / Int32ub), # 78
    "unk_a0" / Int32ub,
    "sound_id" / Int16ub,
    )

# Ex: 02A0
@_pc_res_handler(34)
class _FileMenuHandler:
    name = "File Menu"

    def open(res, container, app):
        newWidget = MyTableWidget("Value")

        parsed = _FileMenuStruct.parse(res.data)
        newWidget.add_row("Scene ID", "0x{:08X}".format(parsed.scene_id))
        newWidget.add_row("Select Game Piece ID", "0x{:08X}".format(parsed.select_game_piece_id))
        newWidget.add_row("Set New ID", "0x{:08X}".format(parsed.set_new_id))
        newWidget.add_row("New ID", "0x{:08X}".format(parsed.new_id))
        newWidget.add_row("Solved ID", "0x{:08X}".format(parsed.solved_id))
        newWidget.add_row("One More ID", "0x{:08X}".format(parsed.one_more_id))
        newWidget.add_row("X More ID", "0x{:08X}".format(parsed.x_more_id))
        newWidget.add_row("XX More ID", "0x{:08X}".format(parsed.xx_more_id))
        newWidget.add_row("Unk @20h", "0x{:08X}".format(parsed.unk_20))
        newWidget.add_row("Unk @24h", "0x{:08X}".format(parsed.unk_24))
        for i in range(0, len(parsed.tens_digit_id)):
            newWidget.add_row("Tens Digit {} ID".format(i), "0x{:08X}".format(parsed.tens_digit_id[i]))
        for i in range(0, len(parsed.ones_digit_id)):
            newWidget.add_row("Ones Digit {} ID".format(i), "0x{:08X}".format(parsed.ones_digit_id[i]))
        for i in range(0, len(parsed.unk_digit_id)):
            newWidget.add_row("Unk Digit {} ID".format(i), "0x{:08X}".format(parsed.unk_digit_id[i]))
        newWidget.add_row("Unk @A0h", "0x{:08X}".format(parsed.unk_a0))
        newWidget.add_row("Sound ID", "0x{:04X}".format(parsed.sound_id))

        container.addWidget(newWidget)

_DifficultyMenuStruct = "_DifficultyMenuStruct" / Struct(
    "scene_id" / Int32ub,
    "choose_difficulty_id" / Int32ub,
    "change_difficulty_id" / Int32ub,
    )

# Ex: 006E
@_pc_res_handler(35)
class _DifficultyMenuHandler:
    name = "Difficulty Menu"

    def open(res, container, app):
        newWidget = MyTableWidget("Value")

        parsed = _DifficultyMenuStruct.parse(res.data)
        newWidget.add_row("Scene ID", "0x{:08X}".format(parsed.scene_id))
        newWidget.add_row("Choose Difficulty ID", "0x{:08X}".format(parsed.choose_difficulty_id))
        newWidget.add_row("Change Difficulty ID", "0x{:08X}".format(parsed.change_difficulty_id))

        container.addWidget(newWidget)

_PotionPuzzleStruct = "_PotionPuzzleStruct" / Struct(
    "unk_0" / Int32ub,
    "bg_image_id" / Int32ub,
    "palette_id" / Int32ub,
    "unk_c" / Int32ub,
    "unk_10" / Int32ub,
    "unk_14" / Int32ub,
    "unk_18" / Int32ub,
    "unk_1c" / Int32ub,
    "unk_20" / Int32ub,
    "unk_24" / Int32ub,
    "unk_28" / Int32ub,
    "unk_2c" / Int32ub,
    "unk_30" / Int32ub,
    "delay" / Int16ub,
    Array(7, "sound_id" / Int16ub),
    "origin_x" / Int16sb,
    "origin_y" / Int16sb,
    )

# Ex: 9C0E
@_pc_res_handler(59)
class _PotionPuzzleHandler:
    name = "Potion Puzzle"

    def open(res, container, app):
        newWidget = MyTableWidget("Value")

        parsed = _PotionPuzzleStruct.parse(res.data)
        newWidget.add_row("Unk @0", "0x{:08X}".format(parsed.unk_0))
        newWidget.add_row("Background Image ID", "0x{:08X}".format(parsed.bg_image_id))
        newWidget.add_row("Palette ID", "0x{:08X}".format(parsed.palette_id))
        newWidget.add_row("Unk @Ch", "0x{:08X}".format(parsed.unk_c))
        newWidget.add_row("Unk @10h", "0x{:08X}".format(parsed.unk_10))
        newWidget.add_row("Unk @14h", "0x{:08X}".format(parsed.unk_14))
        newWidget.add_row("Unk @18h", "0x{:08X}".format(parsed.unk_18))
        newWidget.add_row("Unk @1Ch", "0x{:08X}".format(parsed.unk_1c))
        newWidget.add_row("Unk @20h", "0x{:08X}".format(parsed.unk_20))
        newWidget.add_row("Unk @24h", "0x{:08X}".format(parsed.unk_24))
        newWidget.add_row("Unk @28h", "0x{:08X}".format(parsed.unk_28))
        newWidget.add_row("Unk @2Ch", "0x{:08X}".format(parsed.unk_2c))
        newWidget.add_row("Unk @30h", "0x{:04X}".format(parsed.unk_30))
        newWidget.add_row("Delay", "{} ms".format(parsed.delay))
        for i in range(0, len(parsed.sound_id)):
            newWidget.add_row("Sound {}".format(i+1), "0x{:04X}".format(parsed.sound_id[i]))
        newWidget.add_row("Origin", "({}, {})".format(parsed.origin_x, parsed.origin_y))

        container.addWidget(newWidget)

# Ex: 9C0A
@_pc_res_handler(60)
class _PotionIngredientSlotHandler:
    name = "Potion Ingredient Slot"

# Ex: 9B23
@_pc_res_handler(61)
class _PotionIngredientsHandler:
    name = "Potion Ingredients"

# Ex: 9B22
@_pc_res_handler(62)
class _PotionComboListHandler:
    name = "Potion Combo List"

# Movie list extracted from MERLIN.EXE
_POTION_MOVIE_NAMES = (
    'ELEC', 'EXPL', 'FLAM', 'FLSH', 'MIST', 'OOZE', 'SHMR',
    'SWRL', 'WIND', 'BOIL', 'BUBL', 'BSPK', 'FBRS', 'FCLD',
    'FFLS', 'FSWR', 'LAVA', 'LFIR', 'LSMK', 'SBLS', 'SCLM',
    'SFLS', 'SPRE', 'WSTM', 'WSWL', 'BUGS', 'CRYS', 'DNCR',
    'FISH', 'GLAC', 'GOLM', 'EYEB', 'MOLE', 'MOTH', 'MUDB',
    'ROCK', 'SHTR', 'SLUG', 'SNAK', 'SPKB', 'SPKM', 'SPDR',
    'SQID', 'CLOD', 'SWIR', 'VOLC', 'WORM',
    )

_PotionComboStruct = "_PotionComboStruct" / Struct(
    "a" / Int8ub,
    "b" / Int8ub,
    "c" / Int8ub,
    "d" / Int8ub,
    "movie" / Int16ub,
    )

# Ex: 9B17
@_pc_res_handler(63)
class _PotionCombosHandler:
    name = "Potion Combos"

    def open(res, container, app):
        newWidget = MyTableWidget("A", "B", "C", "D", "Movie")

        parsed = GreedyRange(_PotionComboStruct).parse(res.data)
        for i in range(0, len(parsed)):
            try:
                movie_name = _POTION_MOVIE_NAMES[parsed[i].movie]
            except IndexError:
                movie_name = "Unknown"
            newWidget.add_row("{}".format(i),
                "0x{:02X}".format(parsed[i].a),
                "0x{:02X}".format(parsed[i].b),
                "0x{:02X}".format(parsed[i].c),
                "0x{:02X}".format(parsed[i].d),
                "{} ({})".format(movie_name, parsed[i].movie),
                )

        container.addWidget(newWidget)

class MyHexViewerWidget(QtGui.QTextEdit):
    def __init__(self, data):
        super().__init__()
        self.setReadOnly(True)
        self.setLineWrapMode(QtGui.QTextEdit.NoWrap)
        self.setFontFamily("courier")

        text = ''
        addr = 0
        while addr < len(data):
            text += "{:08X} ".format(addr)

            num_bytes = len(data) - addr
            if num_bytes > 16:
                num_bytes = 16

            for i in range(0, num_bytes):
                if i == 8:
                    text += " "
                text += " {:02X}".format(data[addr + i])

            text += "\n"
            addr += num_bytes

        self.setPlainText(text)

def _open_hex_viewer(res, container, app):
    container.addWidget(MyHexViewerWidget(res.data))

class BltViewer:
    def __init__(self, argv):
        self.cur_palette = _TEST_PALETTE

        self.app = QtGui.QApplication(argv)

        self.win = QtGui.QMainWindow()
        self.win.setWindowTitle("BLT Viewer")

        menuBar = QtGui.QMenuBar()
        fileMenu = menuBar.addMenu("&File")
        fileMenu.addAction("&Open", self._open_action)
        fileMenu.addSeparator()
        fileMenu.addAction("E&xit", self.app.closeAllWindows)

        self.win.setMenuBar(menuBar)

        self.tree = QtGui.QTreeWidget()
        self.tree.setHeaderLabels(("ID", "Type", "Size"))
        self.tree.itemActivated.connect(self._tree_item_activated_action)

        self.content = QtGui.QStackedWidget()
        self.content.addWidget(QtGui.QLabel("Please open a BLT file"))

        splitter = QtGui.QSplitter()
        splitter.addWidget(self.tree)
        splitter.addWidget(self.content)
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)

        self.win.setCentralWidget(splitter)

        self.win.resize(600, 400)

        self.win.show()

    def exec_(self):
        return self.app.exec_()

    def _open_action(self):
        file_dlg_result = QtGui.QFileDialog.getOpenFileName(self.win,
            filter="BLT Files (*.BLT);;All Files (*.*)")
        in_path = file_dlg_result[0]
        if len(in_path) > 0:
            in_file = open(in_path, 'rb')
            self._load_blt_file(in_file)

    def _tree_item_activated_action(self, item, column):
        res_id = item.data(0, Qt.UserRole)
        if res_id is not None:
            print("Opening res id {:04X}...".format(res_id))
            self._load_resource(res_id)

    def _load_blt_file(self, in_file):

        # Clean up widgets
        self.tree.clear()
        self.content.removeWidget(self.content.currentWidget())

        # Open file
        print("Opening BLT file...")
        self.blt_file = BltFile(in_file)

        # Detect platform by file size
        if self.blt_file.file_size in _BOLTLIB_FILE_SIZE_PLATFORMS:
            game_name, self.platform = _BOLTLIB_FILE_SIZE_PLATFORMS[self.blt_file.file_size]
            self.content.addWidget(QtGui.QLabel(
                "Detected game: {}\n"
                "Please double-click on a resource.".format(game_name)
                ))
        else:
            self.content.addWidget(QtGui.QLabel(
                "Detected unknown game (file size: {} bytes). Assuming PC platform.\n"
                "Please double-click on a resource.".format(self.blt_file.file_size)
                ))
            self.platform = "PC"

        # Build resource tree
        for dir in self.blt_file.dir_table:
            dir_item = QtGui.QTreeWidgetItem()
            dir_item.setText(0, dir.name)

            for res in dir.res_table:
                res_item = QtGui.QTreeWidgetItem()
                res_item.setText(0, res.name)
                handler = _RES_TYPE_HANDLERS[self.platform].get(res.type)
                if handler:
                    res_item.setText(1, "{} ({})".format(handler.name, res.type))
                else:
                    res_item.setText(1, "{}".format(res.type))
                res_item.setText(2, "{}".format(res.size))

                res_item.setData(0, Qt.UserRole, res.id)

                dir_item.addChild(res_item)

            self.tree.addTopLevelItem(dir_item)

    def _load_resource(self, res_id):

        # Clean up widgets
        self.content.removeWidget(self.content.currentWidget())

        tabWidget = QtGui.QTabWidget()

        # Load resource
        res = self.blt_file.load_resource(res_id)

        # Create Resource tab via handler if available
        handler = _RES_TYPE_HANDLERS[self.platform].get(res.type)
        if handler and hasattr(handler, "open"):
            resTab = QtGui.QStackedWidget()
            handler.open(res, resTab, self)
            tabWidget.addTab(resTab, "Resource")

        # Create Hex tab
        hexViewerTab = QtGui.QStackedWidget()
        _open_hex_viewer(res, hexViewerTab, self)
        tabWidget.addTab(hexViewerTab, "Hex")

        self.content.addWidget(tabWidget)

def main(argv):
    app = BltViewer(argv)
    return app.exec_()

if __name__=='__main__':
    sys.exit(main(sys.argv))
