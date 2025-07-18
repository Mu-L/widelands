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

#ifndef WL_LOGIC_MAP_OBJECTS_TRIBES_TRAININGSITE_H
#define WL_LOGIC_MAP_OBJECTS_TRIBES_TRAININGSITE_H

#include <memory>

#include "base/macros.h"
#include "logic/map_objects/tribes/productionsite.h"
#include "logic/map_objects/tribes/soldiercontrol.h"
#include "logic/map_objects/tribes/training_attribute.h"

struct TrainingSiteWindow;

namespace Widelands {

class TrainingSiteDescr : public ProductionSiteDescr {
public:
	TrainingSiteDescr(const std::string& init_descname,
	                  const LuaTable& table,
	                  const std::vector<std::string>& attribs,
	                  Descriptions& descriptions);
	~TrainingSiteDescr() override = default;

	[[nodiscard]] Building& create_object() const override;

	[[nodiscard]] Quantity get_max_number_of_soldiers() const {
		return num_soldiers_;
	}
	void set_max_number_of_soldiers(Quantity q) {
		num_soldiers_ = q;
	}
	[[nodiscard]] bool get_train_health() const {
		return train_health_;
	}
	[[nodiscard]] bool get_train_attack() const {
		return train_attack_;
	}
	[[nodiscard]] bool get_train_defense() const {
		return train_defense_;
	}
	[[nodiscard]] bool get_train_evade() const {
		return train_evade_;
	}

	[[nodiscard]] unsigned get_min_level(TrainingAttribute) const;
	[[nodiscard]] unsigned get_max_level(TrainingAttribute) const;
	[[nodiscard]] int32_t get_max_stall() const {
		return max_stall_;
	}
	void set_max_stall(int32_t trainer_patience) {
		max_stall_ = trainer_patience;
	}

	[[nodiscard]] const std::string& no_soldier_to_train_message() const {
		return no_soldier_to_train_message_;
	}

	[[nodiscard]] const std::string& no_soldier_for_training_level_message() const {
		return no_soldier_for_training_level_message_;
	}

private:
	void update_level(TrainingAttribute attrib, unsigned from_level, unsigned to_level);

	//  TODO(unknown): These variables should be per soldier type. They should be in a
	//  struct and there should be a vector, indexed by Soldier_Index,
	//  with that struct structs as element type.
	/** Maximum number of soldiers for a training site */
	Quantity num_soldiers_;
	/** Number of rounds w/o successful training, after which a soldier is kicked out. */
	uint32_t max_stall_;
	/** Whether this site can train health */
	bool train_health_{false};
	/** Whether this site can train attack */
	bool train_attack_{false};
	/** Whether this site can train defense */
	bool train_defense_{false};
	/** Whether this site can train evasion */
	bool train_evade_{false};

	/** Minimum health a soldier needs to train at this site */
	unsigned min_health_{std::numeric_limits<uint32_t>::max()};
	/** Minimum attack a soldier needs to train at this site */
	unsigned min_attack_{std::numeric_limits<uint32_t>::max()};
	/** Minimum defense a soldier needs to train at this site */
	unsigned min_defense_{std::numeric_limits<uint32_t>::max()};
	/** Minimum evade a soldier needs to train at this site */
	unsigned min_evade_{std::numeric_limits<uint32_t>::max()};

	/** Maximum health a soldier can acquire at this site */
	unsigned max_health_{0U};
	/** Maximum attack a soldier can acquire at this site */
	unsigned max_attack_{0U};
	/** Maximum defense a soldier can acquire at this site */
	unsigned max_defense_{0U};
	/** Maximum evasion a soldier can acquire at this site */
	unsigned max_evade_{0U};

	std::string no_soldier_to_train_message_;
	std::string no_soldier_for_training_level_message_;

	DISALLOW_COPY_AND_ASSIGN(TrainingSiteDescr);
};

/**
 * A building to change soldiers' abilities.
 * Soldiers can gain health, or experience in attack, defense and evasion.
 *
 * \note  A training site does not change influence areas. If you lose the
 *        surrounding strongholds, the training site will burn even if it
 *        contains soldiers!
 */
class TrainingSite : public ProductionSite {
	friend class MapBuildingdataPacket;
	MO_DESCR(TrainingSiteDescr)
	friend struct ::TrainingSiteWindow;

	struct Upgrade {
		TrainingAttribute attribute;  // attribute for this upgrade
		std::string prefix;           // prefix for programs
		int32_t min, max;             // minimum and maximum program number (inclusive)
		uint32_t prio;                // relative priority
		uint32_t credit;              // whenever an upgrade gets credit >= 10, it can be run
		int32_t lastattempt;          // level of the last attempt in this upgrade category

		// whether the last attempt in this upgrade category was successful
		bool lastsuccess;
		uint32_t failures;
	};

public:
	explicit TrainingSite(const TrainingSiteDescr&);

	bool init(EditorGameBase&) override;
	void cleanup(EditorGameBase&) override;
	void act(Game&, uint32_t data) override;

	void add_worker(Worker&) override;
	void remove_worker(Worker&) override;
	bool is_present(Worker& worker) const override;

	// TODO(tothxa): These are never used, the variable is always false.
	bool get_build_heroes() const {
		return build_heroes_;
	}
	void set_build_heroes(bool b_heroes) {
		build_heroes_ = b_heroes;
	}
	void switch_heroes();

	bool get_requesting_weak_trainees() const {
		return requesting_weak_trainees_;
	}

	void set_economy(Economy* e, WareWorker type) override;

	int32_t get_pri(enum TrainingAttribute atr);
	void set_pri(enum TrainingAttribute atr, int32_t prio);

	// These are for premature soldier kick-out
	void training_attempted(TrainingAttribute type, uint32_t level);
	void training_successful(TrainingAttribute type, uint32_t level);
	void training_done();
	ProductionProgram::Action::TrainingParameters checked_soldier_training() const;

	std::unique_ptr<const BuildingSettings> create_building_settings() const override;

protected:
	void program_end(Game&, ProgramResult) override;

private:
	class SoldierControl : public Widelands::SoldierControl {
	public:
		explicit SoldierControl(TrainingSite* training_site) : training_site_(training_site) {
		}

		[[nodiscard]] std::vector<Soldier*> present_soldiers() const override;
		[[nodiscard]] std::vector<Soldier*> stationed_soldiers() const override;
		[[nodiscard]] std::vector<Soldier*> associated_soldiers() const override;
		[[nodiscard]] Quantity min_soldier_capacity() const override;
		[[nodiscard]] Quantity max_soldier_capacity() const override;
		[[nodiscard]] Quantity soldier_capacity() const override;
		void set_soldier_capacity(Quantity capacity) override;
		void drop_soldier(Soldier&) override;
		int incorporate_soldier(EditorGameBase& egbase, Soldier& s) override;

	private:
		TrainingSite* const training_site_;
	};
	void update_soldier_request(bool);
	static void
	request_soldier_callback(Game&, Request&, DescriptionIndex, Worker*, PlayerImmovable&);

	void find_and_start_next_program(Game&) override;
	void start_upgrade(Game&, Upgrade&);
	void add_upgrade(TrainingAttribute, const std::string& prefix);
	void calc_upgrades();

	int32_t get_max_unstall_level(TrainingAttribute, const TrainingSiteDescr&) const;
	void drop_unupgradable_soldiers(Game&);
	void drop_stalled_soldiers(Game&);
	Upgrade* get_upgrade(TrainingAttribute);

	SoldierControl soldier_control_;
	/// Open requests for soldiers. The soldiers can be under way or unavailable
	SoldierRequest* soldier_request_{nullptr};

	/** The soldiers currently at the training site*/
	std::vector<Soldier*> soldiers_;

	/** Number of soldiers that should be trained concurrently.
	 * Equal or less to maximum number of soldiers supported by a training site.
	 * There is no guarantee there really are capacity_ soldiers in the
	 * building - some of them might still be under way or even not yet
	 * available*/
	Quantity capacity_;

	/** True, \b always upgrade already experienced soldiers first, when possible
	 * False, \b always upgrade inexperienced soldiers first, when possible */
	bool build_heroes_{false};

	std::vector<Upgrade> upgrades_;
	Upgrade* current_upgrade_;

	ProgramResult result_{ProgramResult::kFailed};  /// The result of the last training program.

	// These are used for kicking out soldiers prematurely
	static const uint32_t training_state_multiplier_;
	// Unuque key to address each training level of each war art

	using TypeAndLevel = std::pair<TrainingAttribute, uint16_t>;
	// First entry is the "stallness", second is a bool
	using FailAndPresence = std::pair<uint16_t, uint8_t>;  // first might wrap in a long play..
	using TrainFailCount = std::map<TypeAndLevel, FailAndPresence>;
	TrainFailCount training_failure_count_;
	uint32_t max_stall_val_;
	// These are for soldier import.
	// If the training site can complete its job, or, in other words, soldiers leave
	// because of they are unupgradeable, then the training site tries to grab already-trained
	// folks in. If the site kicks soldiers off in the middle, it attempts to get poorly trained
	// replacements.
	//
	// Since ALL training sites do this, there needs to be a way to avoid deadlocks.
	// That makes this a bit messy. Sorry.
	//
	// If I was importing strong folks, and switch to weak ones, the switch only happens
	// after ongoing request is (partially) fulfilled. The other direction happens immediately.
	uint8_t highest_trainee_level_seen_;    // When requesting already-trained, start here.
	uint8_t latest_trainee_kickout_level_;  // If I cannot train, request soldiers that have been
	                                        // trainable
	uint8_t trainee_general_lower_bound_;   // This is the acceptance threshold currently in use.
	uint8_t repeated_layoff_ctr_;  // increases when soldier is prematurely releases, reset when
	                               // training succeeds.
	bool repeated_layoff_inc_;
	bool latest_trainee_was_kickout_;  // If soldier was not dropped, requesting new soldier.
	bool requesting_weak_trainees_;    // Value of the previous after incorporate.
	bool recent_capacity_increase_;    // If used explicitly asks for more folks
	const uint8_t kUpperBoundThreshold_ =
	   3;  // Higher value makes it less likely to get weak soldiers in.
	const Duration acceptance_threshold_timeout =
	   Duration(5555);         // Lower the bar after this many milliseconds.
	Time request_open_since_;  // Time units. If no soldiers appear, threshold is lowered after this.
	void init_kick_state(const TrainingAttribute&, const TrainingSiteDescr&);

	ProductionProgram::Action::TrainingParameters checked_soldier_training_;
};

/**
 * Note to be published when a soldier is leaving the training center
 */
// A note we're using to notify the AI
struct NoteTrainingSiteSoldierTrained {
	CAN_BE_SENT_AS_NOTE(NoteId::TrainingSiteSoldierTrained)

	// The trainingsite from where soldier is leaving.
	TrainingSite* ts;

	// The player that owns the ttraining site.
	Player* player;

	NoteTrainingSiteSoldierTrained(TrainingSite* const init_ts, Player* init_player)
	   : ts(init_ts), player(init_player) {
	}
};
}  // namespace Widelands

#endif  // end of include guard: WL_LOGIC_MAP_OBJECTS_TRIBES_TRAININGSITE_H
