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

#ifndef WL_ECONOMY_PORTDOCK_H
#define WL_ECONOMY_PORTDOCK_H

#include <memory>

#include "base/macros.h"
#include "economy/shippingitem.h"
#include "economy/soldier_request.h"
#include "logic/map_objects/immovable.h"
#include "logic/map_objects/tribes/wareworker.h"

namespace Widelands {

struct ShipFleet;
struct RoutingNodeNeighbour;
struct Ship;
class Warehouse;
class ExpeditionBootstrap;

class PortdockDescr : public MapObjectDescr {
public:
	PortdockDescr(char const* init_name, char const* init_descname);
	~PortdockDescr() override = default;

private:
	DISALLOW_COPY_AND_ASSIGN(PortdockDescr);
};

/**
 * The PortDock occupies the fields in the water at which ships
 * dock at a port. As such, this class cooperates closely with
 * @ref Warehouse to implement the port functionality.
 *
 * @ref WareInstance and @ref Worker that are waiting to be
 * transported by ship are stored in the PortDock instead of
 * the associated @ref WareHouse.
 *
 * @paragraph PortDockLifetime
 *
 * The PortDock is created and removed by its owning warehouse.
 * Throughout the life of the PortDock, the corresponding @ref Warehouse
 * instance exists.
 *
 * @paragraph Limitations
 *
 * Currently, there is a 1:1 relationship between @ref Warehouse
 * and PortDock. In principle, it would be conceivable to have a
 * port that is on a land bridge and therefore close to two
 * disconnected bodies of water. Such a port would have to have
 * two PortDock that belong to the same @ref Warehouse, but have
 * separate @ref ShipFleet instances.
 * However, we expect this to be such a rare case that it is not
 * implemented at the moment.
 */
class PortDock : public PlayerImmovable {
public:
	const PortdockDescr& descr() const;

	explicit PortDock(Warehouse* wh);
	~PortDock() override;

	void add_position(Widelands::Coords where);
	[[nodiscard]] Warehouse* get_warehouse() const;

	[[nodiscard]] ShipFleet* get_fleet() const {
		return fleet_;
	}
	[[nodiscard]] PortDock* get_dock(Flag& flag) const;

	void set_economy(Economy*, WareWorker) override;

	[[nodiscard]] int32_t get_size() const override;
	[[nodiscard]] bool get_passable() const override;

	[[nodiscard]] Flag& base_flag() override;
	[[nodiscard]] const Flag& base_flag() const override;
	[[nodiscard]] PositionList get_positions(const EditorGameBase&) const override;

	bool init(EditorGameBase&) override;
	void cleanup(EditorGameBase&) override;

	void add_neighbours(std::vector<RoutingNodeNeighbour>& neighbours);

	void add_shippingitem(Game&, WareInstance&);
	void update_shippingitem(Game&, const WareInstance&);

	void add_shippingitem(Game&, Worker&);
	void update_shippingitem(Game&, const Worker&);

	void shipping_item_arrived(Game&, ShippingItem&);
	void shipping_item_returned(Game&, ShippingItem&);
	void ship_arrived(Game&, Ship&);

	void log_general_info(const EditorGameBase&) const override;

	[[nodiscard]] uint32_t count_waiting(WareWorker waretype, DescriptionIndex wareindex) const;
	[[nodiscard]] uint32_t count_waiting(const PortDock* = nullptr) const;

	// Returns true if a expedition is started or ready to be send out.
	[[nodiscard]] bool expedition_started() const;

	// Called when the button in the warehouse window is pressed.
	void start_expedition();
	void cancel_expedition(Game&);

	// May return nullptr when there is no expedition ongoing or if the
	// expedition ship is already underway.
	[[nodiscard]] ExpeditionBootstrap* expedition_bootstrap() const;

	[[nodiscard]] bool is_expedition_ready() const {
		return expedition_ready_;
	}

	// Gets called by the ExpeditionBootstrap as soon as all wares and workers are available.
	void set_expedition_bootstrap_complete(Game& game, bool complete);

	[[nodiscard]] SoldierRequestManager* get_warship_request_manager(Serial ship) const;
	SoldierRequestManager& create_warship_request_manager(Ship* ship, SoldierPreference pref);
	void erase_warship_request_manager(Serial ship);
	[[nodiscard]] Ship* find_ship_for_warship_request(const EditorGameBase& egbase,
	                                                  const Request& req) const;

private:
	friend class MapBuildingdataPacket;
	friend struct ShipFleet;
	friend struct ShippingSchedule;

	// Does nothing - we do not show them on the map
	void
	draw(const Time&, InfoToDraw, const Vector2f&, const Coords&, float, RenderTarget*) override {
	}

	void init_fleet(EditorGameBase& egbase);
	void set_fleet(ShipFleet* fleet);
	void update_shippingitem(Game&, std::list<ShippingItem>::iterator);

	bool load_one_item(Game&, Ship&, const PortDock& dest);

	[[nodiscard]] uint32_t calc_max_priority(const EditorGameBase&, const PortDock& dest) const;

	ShipFleet* fleet_{nullptr};
	Warehouse* warehouse_;
	PositionList dockpoints_;
	std::list<ShippingItem> waiting_;
	bool expedition_ready_{false};
	bool expedition_cancelling_{false};

	std::unique_ptr<ExpeditionBootstrap> expedition_bootstrap_;

	std::map<Serial, std::unique_ptr<SoldierRequestManager>> warship_soldier_request_managers_;

	// saving and loading
protected:
	class Loader : public PlayerImmovable::Loader {
	public:
		Loader() = default;

		void load(FileRead&, uint8_t packet_version);
		void load_pointers() override;
		void load_finish() override;

	private:
		uint32_t warehouse_{0U};
		std::vector<ShippingItem::Loader> waiting_;
	};

public:
	bool has_new_save_support() override {
		return true;
	}
	void save(EditorGameBase&, MapObjectSaver&, FileWrite&) override;

	static MapObject::Loader* load(EditorGameBase&, MapObjectLoader&, FileRead&);
};

extern PortdockDescr g_portdock_descr;

}  // namespace Widelands

#endif  // end of include guard: WL_ECONOMY_PORTDOCK_H
