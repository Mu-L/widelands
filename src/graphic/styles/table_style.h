/*
 * Copyright (C) 2018-2025 by the Widelands Development Team
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

#ifndef WL_GRAPHIC_STYLES_TABLE_STYLE_H
#define WL_GRAPHIC_STYLES_TABLE_STYLE_H

#include <memory>

#include "graphic/styles/font_style.h"

namespace UI {

struct TableStyleInfo {
	explicit TableStyleInfo(UI::FontStyleInfo* init_enabled,
	                        UI::FontStyleInfo* init_disabled,
	                        UI::FontStyleInfo* init_hotkey)
	   : enabled_(init_enabled), disabled_(init_disabled), hotkey_(init_hotkey) {
	}

	[[nodiscard]] const UI::FontStyleInfo& enabled() const {
		return *enabled_;
	}
	[[nodiscard]] const UI::FontStyleInfo& disabled() const {
		return *disabled_;
	}
	[[nodiscard]] const UI::FontStyleInfo& hotkey() const {
		return *hotkey_;
	}

private:
	std::unique_ptr<const UI::FontStyleInfo> enabled_;
	std::unique_ptr<const UI::FontStyleInfo> disabled_;
	std::unique_ptr<const UI::FontStyleInfo> hotkey_;
};

}  // namespace UI

#endif  // end of include guard: WL_GRAPHIC_STYLES_TABLE_STYLE_H
