/*
 * Copyright (C) 2002-2025 by the Widelands Development Team
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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "wui/savegamedata.h"

#include "base/i18n.h"
#include "base/string.h"
#include "base/time_string.h"
#include "graphic/text_layout.h"

SavegameData::SavegameData(const std::string& fname)
   : SavegameData(fname, SavegameType::kSavegame) {
}
SavegameData::SavegameData(const std::string& fname, const SavegameType& type)
   : filename(fname),
     nrplayers("0"),
     savetimestamp(0),
     gametype(GameController::GameType::kSingleplayer),
     type_(type) {
}

void SavegameData::set_gametime(uint32_t input_gametime) {
	gametime = gametimestring(input_gametime);
}
void SavegameData::set_nrplayers(Widelands::PlayerNumber input_nrplayers) {
	nrplayers = as_string(static_cast<unsigned int>(input_nrplayers));
}
void SavegameData::set_mapname(const std::string& input_mapname) {
	// TODO(Nordfriese): If the map was defined by an add-on, use that add-on's textdomain
	// instead (if available). We'll need to store the add-on name in the savegame for this.
	i18n::Textdomain td("maps");
	mapname = _(input_mapname);
}

bool SavegameData::is_directory() const {
	return is_sub_directory() || is_parent_directory();
}

bool SavegameData::is_parent_directory() const {
	return type_ == SavegameType::kParentDirectory;
}

bool SavegameData::is_sub_directory() const {
	return type_ == SavegameType::kSubDirectory;
}

bool SavegameData::is_replay() const {
	return gametype == GameController::GameType::kReplay;
}

bool SavegameData::is_singleplayer() const {
	return gametype == GameController::GameType::kSingleplayer;
}

bool SavegameData::is_multiplayer() const {
	return is_multiplayer_host() || is_multiplayer_client();
}

bool SavegameData::is_multiplayer_host() const {
	return gametype == GameController::GameType::kNetHost;
}

bool SavegameData::is_multiplayer_client() const {
	return gametype == GameController::GameType::kNetClient;
}

bool SavegameData::compare_save_time(const SavegameData& other) const {
	if (is_directory() || other.is_directory()) {
		return compare_directories(other);
	}
	return savetimestamp < other.savetimestamp;
}

bool SavegameData::compare_map_name(const SavegameData& other) const {
	if (is_directory() || other.is_directory()) {
		return compare_directories(other);
	}
	return mapname < other.mapname;
}

bool SavegameData::compare_directories(const SavegameData& other) const {
	// parent directory always on top
	if (is_parent_directory()) {
		return false;
	}
	if (other.is_parent_directory()) {
		return true;
	}
	// sub directory before non-sub directory (aka actual savegame)
	if (is_sub_directory() && !other.is_directory()) {
		return false;
	}
	if (!is_sub_directory() && other.is_sub_directory()) {
		return true;
	}
	// sub directories sort after name
	if (is_sub_directory() && other.is_sub_directory()) {
		return filename > other.filename;
	}

	return false;
}

// static
SavegameData SavegameData::create_parent_dir(const std::string& current_dir) {
	std::string filename = FileSystem::fs_dirname(current_dir);
	if (!filename.empty()) {
		// fs_dirname always returns a directory with a separator at the end.
		filename.pop_back();
	}
	return SavegameData(filename, SavegameData::SavegameType::kParentDirectory);
}

SavegameData SavegameData::create_sub_dir(const std::string& directory) {
	return SavegameData(directory, SavegameData::SavegameType::kSubDirectory);
}

const std::string as_filename_list(const std::vector<SavegameData>& savefiles) {
	std::string message;
	for (const SavegameData& gamedata : savefiles) {
		if (gamedata.is_directory() || !gamedata.errormessage.empty()) {
			message = format("%s\n%s", message, richtext_escape(gamedata.filename));
		} else if (gamedata.errormessage.empty()) {
			std::vector<std::string> listme;
			listme.push_back(richtext_escape(gamedata.mapname));
			listme.push_back(gamedata.savedonstring);
			message =
			   format("%s\n%s", message, i18n::localize_list(listme, i18n::ConcatenateWith::COMMA));
		}
	}
	return message;
}
