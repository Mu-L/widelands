/*
 * Copyright (C) 2011-2025 by the Widelands Development Team
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

#ifndef WL_LOGIC_MAP_OBJECTS_FINDBOB_H
#define WL_LOGIC_MAP_OBJECTS_FINDBOB_H

#include "logic/map.h"

namespace Widelands {

class Game;
class Player;

struct FindBobAttribute : public FindBob {
	explicit FindBobAttribute(uint32_t const init_attrib) : attrib(init_attrib) {
	}

	bool accept(Bob*) const override;

	uint32_t attrib;
	~FindBobAttribute() override = default;  // make gcc shut up
};

/**
 * Find soldiers which are hostile to the given player (or all soldiers
 * if player is 0).
 */
struct FindBobEnemySoldier : public FindBob {
	explicit FindBobEnemySoldier(Player* init_player) : player(init_player) {
	}

	bool accept(Bob*) const override;

private:
	Player* player;
};

struct FindBobShip : FindBob {
	bool accept(Bob* bob) const override;
};

struct FindBobCritter : FindBob {
	bool accept(Bob* bob) const override;
};

/**
 * Accept Bob when is a Soldier alive that is attacking the Player.
 */
struct FindBobSoldierAttackingPlayer : public FindBob {
	FindBobSoldierAttackingPlayer(Game& g, Player& p) : player(p), game(g) {
	}

	[[nodiscard]] bool accept(Bob* bob) const override;

private:
	Player& player;
	Game& game;
};

}  // namespace Widelands

#endif  // end of include guard: WL_LOGIC_MAP_OBJECTS_FINDBOB_H
