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

#ifndef WL_LOGIC_MAP_OBJECTS_TRIBES_BUILDING_SETTINGS_H
#define WL_LOGIC_MAP_OBJECTS_TRIBES_BUILDING_SETTINGS_H

#include <map>
#include <string>

#include "economy/ware_priority.h"
#include "logic/widelands.h"

class FileRead;
class FileWrite;

namespace Widelands {

class Game;
class MarketDescr;
class MilitarySiteDescr;
class ProductionSiteDescr;
enum class StockPolicy;
class TrainingSiteDescr;
class TribeDescr;
class WarehouseDescr;

struct BuildingSettings {
	BuildingSettings(const std::string& name, const TribeDescr& tribe)
	   : tribe_(tribe), descr_(name) {
	}
	virtual ~BuildingSettings() = default;

	static BuildingSettings* load(const Game&, const TribeDescr&, FileRead&);

	virtual void save(const Game&, FileWrite&) const;
	virtual void read(const Game&, FileRead&);

	virtual void apply(const BuildingSettings&) {
	}

protected:
	const TribeDescr& tribe_;

private:
	const std::string descr_;
};

struct ProductionsiteSettings : public BuildingSettings {
	ProductionsiteSettings(const ProductionSiteDescr& descr, const TribeDescr& tribe);
	~ProductionsiteSettings() override = default;
	void apply(const BuildingSettings&) override;

	void save(const Game&, FileWrite&) const override;
	void read(const Game&, FileRead&) override;

	struct InputQueueSetting {
		const uint32_t max_fill;
		uint32_t desired_fill;
		WarePriority priority;
	};
	std::map<DescriptionIndex, InputQueueSetting> ware_queues;
	std::map<DescriptionIndex, InputQueueSetting> worker_queues;
	bool stopped{false};
};

struct MilitarysiteSettings : public BuildingSettings {
	MilitarysiteSettings(const MilitarySiteDescr&, const TribeDescr& tribe);
	~MilitarysiteSettings() override = default;
	void apply(const BuildingSettings&) override;

	void save(const Game&, FileWrite&) const override;
	void read(const Game&, FileRead&) override;

	const uint32_t max_capacity;
	uint32_t desired_capacity;
	SoldierPreference soldier_preference;
};

struct TrainingsiteSettings : public ProductionsiteSettings {
	TrainingsiteSettings(const TrainingSiteDescr&, const TribeDescr& tribe);
	~TrainingsiteSettings() override = default;
	void apply(const BuildingSettings&) override;

	void save(const Game&, FileWrite&) const override;
	void read(const Game&, FileRead&) override;

	const uint32_t max_capacity;
	uint32_t desired_capacity;
};

struct WarehouseSettings : public BuildingSettings {
	WarehouseSettings(const WarehouseDescr&, const TribeDescr& tribe);
	~WarehouseSettings() override = default;
	void apply(const BuildingSettings&) override;

	void save(const Game&, FileWrite&) const override;
	void read(const Game&, FileRead&) override;

	std::map<DescriptionIndex, StockPolicy> ware_preferences;
	std::map<DescriptionIndex, StockPolicy> worker_preferences;
	const bool launch_expedition_allowed;
	bool launch_expedition{false};
	uint32_t desired_capacity{0U};
	const uint32_t max_garrison;
	SoldierPreference soldier_preference{SoldierPreference::kAny};
};

struct MarketSettings : public BuildingSettings {
	MarketSettings(const MarketDescr&, const TribeDescr& tribe);
	~MarketSettings() override = default;
	void apply(const BuildingSettings&) override;

	void save(const Game&, FileWrite&) const override;
	void read(const Game&, FileRead&) override;
};

}  // namespace Widelands

#endif  // end of include guard: WL_LOGIC_MAP_OBJECTS_TRIBES_BUILDING_SETTINGS_H
