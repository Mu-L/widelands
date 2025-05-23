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

#ifndef WL_WUI_TRAININGSITEWINDOW_H
#define WL_WUI_TRAININGSITEWINDOW_H

#include "logic/map_objects/tribes/trainingsite.h"
#include "wui/productionsitewindow.h"

/**
 * Status window for \ref TrainingSite
 */
struct TrainingSiteWindow : public ProductionSiteWindow {
	TrainingSiteWindow(InteractiveBase& parent,
	                   BuildingWindow::Registry& reg,
	                   Widelands::TrainingSite&,
	                   bool avoid_fastclick,
	                   bool workarea_preview_wanted);

private:
	void init(bool avoid_fastclick, bool workarea_preview_wanted) override;

	Widelands::OPtr<Widelands::TrainingSite> training_site_;

	DISALLOW_COPY_AND_ASSIGN(TrainingSiteWindow);
};

#endif  // end of include guard: WL_WUI_TRAININGSITEWINDOW_H
