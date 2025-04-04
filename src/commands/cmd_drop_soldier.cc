/*
 * Copyright (C) 2004-2025 by the Widelands Development Team
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

#include "commands/cmd_drop_soldier.h"

#include "logic/game.h"
#include "logic/game_data_error.h"
#include "logic/map_objects/tribes/ship.h"
#include "logic/map_objects/tribes/soldier.h"
#include "logic/player.h"
#include "map_io/map_object_loader.h"
#include "map_io/map_object_saver.h"

namespace Widelands {

/*** class Cmd_DropSoldier ***/

CmdDropSoldier::CmdDropSoldier(StreamRead& des) : PlayerCommand(Time(0), des.unsigned_8()) {
	serial = des.unsigned_32();   //  Serial of the building
	soldier = des.unsigned_32();  //  Serial of soldier
}

void CmdDropSoldier::execute(Game& game) {
	if (upcast(PlayerImmovable, player_imm, game.objects().get_object(serial))) {
		if (upcast(Soldier, s, game.objects().get_object(soldier))) {
			game.get_player(sender())->drop_soldier(*player_imm, *s);
		}
	} else if (upcast(Ship, ship, game.objects().get_object(serial))) {
		ship->drop_soldier(game, soldier);
	}
}

void CmdDropSoldier::serialize(StreamWrite& ser) {
	write_id_and_sender(ser);
	ser.unsigned_32(serial);
	ser.unsigned_32(soldier);
}

constexpr uint16_t kCurrentPacketVersionCmdDropSoldier = 1;

void CmdDropSoldier::read(FileRead& fr, EditorGameBase& egbase, MapObjectLoader& mol) {
	try {
		const uint16_t packet_version = fr.unsigned_16();
		if (packet_version == kCurrentPacketVersionCmdDropSoldier) {
			PlayerCommand::read(fr, egbase, mol);
			serial = get_object_serial_or_zero<MapObject>(fr.unsigned_32(), mol);
			soldier = get_object_serial_or_zero<Soldier>(fr.unsigned_32(), mol);
		} else {
			throw UnhandledVersionError(
			   "CmdDropSoldier", packet_version, kCurrentPacketVersionCmdDropSoldier);
		}
	} catch (const WException& e) {
		throw GameDataError("drop soldier: %s", e.what());
	}
}

void CmdDropSoldier::write(FileWrite& fw, EditorGameBase& egbase, MapObjectSaver& mos) {
	// First, write version
	fw.unsigned_16(kCurrentPacketVersionCmdDropSoldier);
	// Write base classes
	PlayerCommand::write(fw, egbase, mos);

	//  site serial
	fw.unsigned_32(mos.get_object_file_index_or_zero(egbase.objects().get_object(serial)));

	//  soldier serial
	fw.unsigned_32(mos.get_object_file_index_or_zero(egbase.objects().get_object(soldier)));
}

}  // namespace Widelands
