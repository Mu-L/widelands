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

#include "graphic/mouse_cursor.h"

#include <string>

#include <SDL_events.h>

#include "graphic/image_cache.h"
#include "graphic/image_io.h"
#include "graphic/rendertarget.h"
#include "io/filesystem/layered_filesystem.h"

const std::string default_filename = "images/ui_basic/cursor.png";
const std::string default_click_filename = "images/ui_basic/cursor_click.png";
constexpr int default_hotspot_x = 3;
constexpr int default_hotspot_y = 7;

const std::string default_wait_filename = "images/ui_basic/cursor_wait.png";
constexpr int wait_hotspot_x = 15;
constexpr int wait_hotspot_y = 23;

MouseCursor* g_mouse_cursor;

void MouseCursor::initialize(bool init_use_sdl) {
	default_cursor_ = g_image_cache->get(default_filename);
	default_cursor_click_ = g_image_cache->get(default_click_filename);
	default_cursor_wait_ = g_image_cache->get(default_wait_filename);

	default_cursor_sdl_surface_ = load_image_as_sdl_surface(default_filename, g_fs);
	default_cursor_sdl_ =
	   SDL_CreateColorCursor(default_cursor_sdl_surface_, default_hotspot_x, default_hotspot_y);
	default_cursor_click_sdl_surface_ = load_image_as_sdl_surface(default_click_filename, g_fs);
	default_cursor_click_sdl_ = SDL_CreateColorCursor(
	   default_cursor_click_sdl_surface_, default_hotspot_x, default_hotspot_y);
	default_cursor_wait_sdl_surface_ = load_image_as_sdl_surface(default_wait_filename, g_fs);
	default_cursor_wait_sdl_ =
	   SDL_CreateColorCursor(default_cursor_wait_sdl_surface_, wait_hotspot_x, wait_hotspot_y);

	set_use_sdl(init_use_sdl);
}

MouseCursor::~MouseCursor() {
	SDL_FreeCursor(default_cursor_sdl_);
	SDL_FreeSurface(default_cursor_sdl_surface_);
	SDL_FreeCursor(default_cursor_click_sdl_);
	SDL_FreeSurface(default_cursor_click_sdl_surface_);
	SDL_FreeCursor(default_cursor_wait_sdl_);
	SDL_FreeSurface(default_cursor_wait_sdl_surface_);
}

void MouseCursor::set_use_sdl(bool init_use_sdl) {
	use_sdl_ = init_use_sdl;

	if (use_sdl_) {
		SDL_ShowCursor(visible_ ? SDL_ENABLE : SDL_DISABLE);
		if (is_wait_) {
			SDL_SetCursor(default_cursor_wait_sdl_);
		} else {
			SDL_SetCursor(was_pressed_ ? default_cursor_click_sdl_ : default_cursor_sdl_);
		}
	} else {
		SDL_ShowCursor(SDL_DISABLE);
	}
}

bool MouseCursor::is_using_sdl() const {
	return use_sdl_;
}

void MouseCursor::set_visible(bool visible) {
	if (visible_ == visible) {
		return;
	}
	visible_ = visible;
	if (use_sdl_) {
		SDL_ShowCursor(visible_ ? SDL_ENABLE : SDL_DISABLE);
	}
}

bool MouseCursor::is_visible() const {
	return visible_;
}

void MouseCursor::change_cursor(bool is_pressed) {
	if (use_sdl_ && !is_wait_ && (was_pressed_ != is_pressed)) {
		SDL_SetCursor(is_pressed ? default_cursor_click_sdl_ : default_cursor_sdl_);
	}
	was_pressed_ = is_pressed;
}

// Can be set anywhere without worrying about resetting: panel::do_run() resets it whenever
// user input can actually be processed.
// TODO(tothxa): This cursor should also be used during forced pauses
void MouseCursor::change_wait(bool to_wait) {
	if (is_wait_ == to_wait) {
		return;
	}
	is_wait_ = to_wait;
	if (use_sdl_) {
		if (is_wait_) {
			SDL_SetCursor(default_cursor_wait_sdl_);
		} else {
			SDL_SetCursor(was_pressed_ ? default_cursor_click_sdl_ : default_cursor_sdl_);
		}
	}
}

void MouseCursor::draw(RenderTarget& rt, Vector2i position) {
	if (!use_sdl_ && visible_) {
		if (is_wait_) {
			rt.blit((position - Vector2i(wait_hotspot_x, wait_hotspot_y)), default_cursor_wait_);
		} else {
			rt.blit((position - Vector2i(default_hotspot_x, default_hotspot_y)),
			        was_pressed_ ? default_cursor_click_ : default_cursor_);
		}
	}
}
