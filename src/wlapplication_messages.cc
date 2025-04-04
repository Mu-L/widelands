/*
 * Copyright (C) 2012-2025 by the Widelands Development Team
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

#include "wlapplication_messages.h"

#include <algorithm>
#include <iostream>
#include <regex>

#include "base/i18n.h"
#include "base/string.h"

constexpr size_t kIndent = 23;
constexpr size_t kTextWidth = 50;

#ifndef _WIN32
#ifdef USE_XDG
static const std::string kDefaultHomedir = "~/.local/share/widelands";
#else
static const std::string kDefaultHomedir = "~/.widelands";
#endif
#else
static const std::string kDefaultHomedir = "%USERPROFILE%\\.widelands";
#endif

/// Command line help
/// Title: unindented text in the line above
/// Key: the actual parameter
/// Hint: text after =
/// Help: Full text help
/// Verbose: Filter some config options (--help vs. --help-all)
static std::vector<Parameter> parameters;
void fill_parameter_vector() {
	i18n::Textdomain textdomain("widelands_console");

	/** TRANSLATORS: Separator for alternative values for command line parameters */
	const std::string alternatives_format = _("%1$s|%2$s");

	// TODO(tothxa): Replace all uses and do this for true|false and others as well
	const std::string filename_placeholder = _("FILENAME");

	/** TRANSLATORS: Used instead of a file name indicating last savegame, replay or map.
	    Use '_' instead of spaces if you need multiple words and don't use punctuation marks
	 */
	const std::string lastused_word = _("last");

	const std::string file_or_last_placeholder =
	   format(alternatives_format, filename_placeholder, lastused_word);

	parameters = {
	   {_("Usage:"), _("widelands <option0>=<value0> ... <optionN>=<valueN>"), "--", "", false},
	   {"", _("widelands <save.wgf>/<replay.wry>"), "--", "", false},
	   /// Paths
	   {_("Options:"), "datadir", _("DIRNAME"),
	    _("Use the specified directory for the Widelands data files."), false},
	   {"", "homedir", _("DIRNAME"),
	    format(_("Use the specified directory for Widelands config files, savegames, and replays. "
	             "Default is `%s`."),
	           kDefaultHomedir),
	    false},
	   {"", "localedir", _("DIRNAME"),
	    _("Use the specified directory for the Widelands locale files."), false},
	   {"", "language",
	    /** TRANSLATORS: The … is not used on purpose to increase readability on monospaced terminals
	     */
	    _("[de_DE|sv_SE|...]"), _("Use the specified locale."), false},
	   {"", "skip_check_datadir_version", "",
	    _("Do not check whether the data directory to use is "
	      "compatible with this Widelands version."),
	    true},
	   /// Game setup
	   {"", "new_game_from_template", _("FILENAME"),
	    format(_("Create a new game directly with the settings configured in the given file. "
	             "An example can be found in `%s`."),
	           "data/templates/new_game_template"),
	    false},
	   {"", "scenario", _("FILENAME"),
	    _("Start the map `FILENAME` directly as a singleplayer scenario."), false},
	   {"", "loadgame", file_or_last_placeholder,
	    /** TRANSLATORS: %1 is translation for FILENAME,
	                     %2 is translation for "last" for last used file */
	    format(_("Load the savegame `%1$s` directly or the last saved game if `=%2$s` is used."),
	           filename_placeholder, lastused_word),
	    false},
	   {"", "replay", file_or_last_placeholder,
	    /** TRANSLATORS: %1 is translation for FILENAME,
	                     %2 is translation for "last" for last used file */
	    format(_("Load the replay `%1$s` directly or the last saved replay if `=%2$s` is used."),
	           filename_placeholder, lastused_word),
	    false},
	   {"", "editor", "",
	    /** TRANSLATORS: %1 is translation for FILENAME,
	                     %2 is translation for "last" for last used file */
	    format(_("Start the Widelands map editor directly. You can add `=%1$s` to directly load the "
	             "map `FILENAME` in the editor or `=%2$s` to load the last edited map."),
	           filename_placeholder, lastused_word),
	    false},
	   {"", "script", _("FILENAME"),
	    _("Run the given Lua script after initialization. Only valid with --scenario, --loadgame, "
	      "or "
	      "--editor."),
	    false},
	   {"", "difficulty",
	    /** TRANSLATORS: A placeholder for a numerical value */
	    _("n"),
	    /** TRANSLATORS: `n` references a numerical placeholder */
	    _("Start the scenario with difficulty `n`. Only valid with --scenario."), false},
	   /// Misc
	   {"", "nosound", "", _("Start the game with sound disabled."), false},
	   /** TRANSLATORS: You may translate true/false, also as on/off or yes/no, but */
	   /** TRANSLATORS: it HAS TO BE CONSISTENT with the translation in the widelands textdomain. */
	   /** TRANSLATORS: * marks the default value */
	   {"", "play_intro_music", _("[true*|false]"),
	    _("Play the intro music at startup and show splash image until it ends."), true},
	   {"", "fail-on-lua-error", "", _("Force Widelands to crash when a Lua error occurs."), false},
	   {"", "fail-on-errors", "",
	    _("Force Widelands to crash when a game or the editor terminates with an error."), false},
	   {"", "messagebox-timeout",
	    /** TRANSLATORS: Placeholder for a time value in seconds */
	    _("<seconds>"),
	    _("Automatically close modal message boxes after the given number of seconds time."), true},
	   {"", "replay_lifetime", _("n"), _("Delete replays automatically after `n` weeks."), true},
	   {"", "ai_training", "",
	    _("Enable AI training mode. See https://www.widelands.org/wiki/Ai%20Training/ for a full "
	      "description of the AI training logic."),
	    true},
	   {"", "auto_speed", "",
	    _("Constantly adjust the game speed automatically depending on AI delay. "
	      "Only to be used for AI testing or training (in conjunction with --ai_training)."),
	    true},
	   /* This is deliberately so long to discourage overusage */
	   {"", "enable_development_testing_tools", "",
	    _("Enable the Script Console and Cheating Mode."), true},
	   /// Saving options
	   {_("Game options:"), _("Note: New values will be written to the config file."), "--", "",
	    false},
	   {"", "autosave",
	    /** TRANSLATORS: A placeholder for a numerical value */
	    _("n"),
	    /** TRANSLATORS: `n` references a numerical placeholder */
	    _("Automatically save each `n` minutes."), false},
	   {"", "rolling_autosave", _("n"),
	    /** TRANSLATORS: `n` references a numerical placeholder */
	    _("Use `n` files for rolling autosaves."), true},
	   {"", "skip_autosave_on_inactivity", _("[true*|false]"),
	    _("Do not create an autosave when the user has been inactive since the last autosave."),
	    true},
	   {"", "nozip", "", _("Do not save files as binary zip archives."), false},
	   {"", "zip", "", _("Save files as binary zip archives."), false},
	   // The below comment is duplicated from above for the other case, when false is the default.
	   /** TRANSLATORS: You may translate true/false, also as on/off or yes/no, but */
	   /** TRANSLATORS: it HAS TO BE CONSISTENT with the translation in the widelands textdomain. */
	   /** TRANSLATORS: * marks the default value */
	   {"", "save_chat_history", _("[true|false*]"),
	    _("Whether to save the history of sent chat messages to a file."), true},
	   {"", "display_replay_filenames", _("[true*|false]"),
	    _("Show filenames in the replay screen."), true},
	   {"", "editor_player_menu_warn_too_many_players", _("[true*|false]"),
	    _("Whether a warning should be shown in the editor if there are too many players."), true},
	   /// Game options
	   {"", "pause_game_on_inactivity", _("n"),
	    /** TRANSLATORS: `n` references a numerical placeholder */
	    _("Pause the game after `n` minutes of user inactivity."), true},
	   {"", "auto_roadbuild_mode", _("[true*|false]"),
	    _("Start building a road after placing a flag."), true},
	   {"", "display_flags",
	    /** TRANSLATORS: The … character is not used on purpose to increase readability on monospaced
	       terminals */
	    _("[...]"), _("Bitmask of display flags to set for new games."), true},
#if 0  // TODO(matthiakl): Re-add training wheels code after v1.0
	{"",
	 "training_wheels",
	 _("[true*|false]"),
	 "",
	 true
	},
#endif
	   {"", "edge_scrolling", _("[true|false*]"),
	    _("Scroll when the mouse cursor is near the screen edge."), true},
	   {"", "invert_movement", _("[true|false*]"),
	    _("Invert click-and-drag map movement direction."), true},
	   {"", "numpad_diagonalscrolling", _("[true|false*]"),
	    _("Allow diagonal scrolling with the numeric keypad."), true},
	   {"", "game_clock", _("[true|false*]"), _("Display system time in the info panel."), true},
	   {"", "single_watchwin", _("[true|false*]"), _("Use single watchwindow mode."), true},
	   {"", "transparent_chat", _("[true*|false]"),
	    _("Show in-game chat with transparent background."), true},
	   {"", "toolbar_pos", _("[...]"), _("Bitmask to set the toolbar location and mode."), true},
	   /// Networking
	   {_("Networking:"), "metaserver", _("URI"),
	    _("Connect to a different metaserver for internet gaming."), false},
	   {"", "metaserverport", _("n"),
	    /** TRANSLATORS: `n` references a numerical placeholder */
	    _("Port number `n` of the metaserver for internet gaming."), false},
	   {"", "servername", _("[...]"), _("The name of the last hosted game."), true},
	   {"", "nickname", _("[...]"), _("The nickname used for LAN and online games."), true},
	   {"", "realname", _("[...]"), _("Name of map author."), true},
	   {"", "lasthost", _("[...]"), _("The last host connected to."), true},
	   {"", "registered", _("[true|false*]"),
	    _("Whether the used metaserver login is for a registered user."), true},
	   {"", "password_sha1", _("[...]"), _("The hashed password for online logins."), true},
	   {"", "addon_server_ip", _("IP"),
	    _("Connect to a different server address from the add-ons manager."), false},
	   {"", "addon_server_port", _("n"),
	    _("Connect to a different server port from the add-ons manager."), false},
	   {"", "write_syncstreams", "",
	    /** TRANSLATORS: A syncstream is a synchronization stream. Syncstreams are used in
	     * multiplayer
	     */
	    /** TRANSLATORS: games to make sure that there is no mismatch between the players. */
	    _("Create syncstream dump files to help debug network games."), true},

	   /// Interface options
	   {_("Graphic options:"), "fullscreen", "", _("Use the whole display for the game screen."),
	    false},
	   {"", "maximized", "", _("Start the game in a maximized window."), false},
	   {"", "xres",
	    /** TRANSLATORS: A placeholder for window width */
	    _("x"),
	    /** TRANSLATORS: `x` references a window width placeholder */
	    _("Width `x` of the window in pixel."), false},
	   {"", "yres",
	    /** TRANSLATORS: A placeholder for window height */
	    _("y"),
	    /** TRANSLATORS: `y` references a window height placeholder */
	    _("Height `y` of the window in pixel."), false},
	   {"", "sdl_cursor", _("[true*|false]"),
	    _("Whether to let the system draw the mouse cursor. Disable it only if the cursor doesn't "
	      "appear right, or if you want it to be visible in screenshots or screencasts."),
	    true},
	   {"", "tooltip_accessibility_mode", _("[true|false*]"), _("Whether to use sticky tooltips."),
	    true},
	   {"", "theme", _("DIRNAME"),
	    _("The path to the active UI theme, relative to the Widelands home directory."), false},
	   /// Window options
	   {_("Options for the internal window manager:"), "animate_map_panning", _("[true*|false]"),
	    _("Whether automatic map movements should be animated."), true},
	   {"", "border_snap_distance", _("n"),
	    /** TRANSLATORS: `n` references a numerical placeholder */
	    _("Move a window to the edge of the screen when the edge of the window comes within a "
	      "distance `n` from the edge of the screen."),
	    true},
	   {"", "dock_windows_to_edges", _("[true|false*]"),
	    _("Eliminate a window’s border towards the edge of the screen when the edge of the window "
	      "is "
	      "next to the edge of the screen."),
	    true},
	   {"", "panel_snap_distance", _("n"),
	    /** TRANSLATORS: `n` references a numerical placeholder */
	    _("Move a window to the edge of the panel when the edge of the window comes within "
	      "a distance of `n` from the edge of the panel."),
	    true},
	   /// Others
	   {_("Others:"), "verbose", "", _("Enable verbose debug messages"), false},
	   {"", "verbose-i18n", "",
	    _("Print all strings as they are translated. "
	      "This helps with tracing down bugs with internationalization."),
	    true},
	   {"", "version", "", _("Only print version and exit."), false},
	   {"", "help", "", _("Show this help."), false},
	   {"", "help-all", "", _("Show this help with all available config options."), false},
	   {"", _("<save.wgf>/<replay.wry>"), "--",
	    _("Load the given savegame or replay directly. Useful for .wgf/.wry file extension "
	      "association. Does not work with other options. Also see --loadgame/--replay."),
	    false}};
}

const std::vector<std::string> get_all_parameters() {
	std::vector<std::string> result;
	for (const Parameter& param : parameters) {
		// Filter out special entries
		if (param.hint_ != "--") {
			result.emplace_back(param.key_);
		}
	}
	return result;
}

bool is_parameter(const std::string& name) {
	auto result = std::find_if(
	   parameters.begin(), parameters.end(), [name](const Parameter& p) { return p.key_ == name; });
	return result != parameters.end() && result->hint_ != "--";
}

bool use_last(const std::string& filename_arg) {
	return i18n::is_translation_of(filename_arg, "last", "widelands_console");
}

/**
 * Print usage information
 */
void show_usage(const std::string& build_ver_details, CmdLineVerbosity verbosity) {
	i18n::Textdomain textdomain("widelands_console");

	std::cout << std::string(kIndent + kTextWidth, '=')
	          << std::endl
	          /** TRANSLATORS: %s = version information */
	          << format(_("This is Widelands version %s"), build_ver_details) << std::endl;

	if (verbosity != CmdLineVerbosity::None) {
		std::string indent_string = std::string(kIndent, ' ');
		bool multiline = true;
		for (const Parameter& param : parameters) {
			if (verbosity != CmdLineVerbosity::All && param.is_verbose_) {
				continue;
			}

			if (!param.title_.empty()) {
				std::cout << std::endl << param.title_ << std::endl;
			} else if (!multiline) {
				// Space out single line entries
				std::cout << std::endl;
			}

			std::string column = " ";
			if (param.hint_ == "--") {
				// option without dashes
				column += param.key_;
			} else {
				column += std::string("--") + param.key_;
				if (!param.hint_.empty()) {
					column += std::string("=") + param.hint_;
				}
			}

			std::cout << column;
			if (param.help_.empty()) {
				std::cout << std::endl;
				continue;
			}

			multiline = column.size() >= kIndent;
			if (multiline) {
				std::cout << std::endl << indent_string;
			} else {
				std::cout << std::string(kIndent - column.size(), ' ');
			}

			std::string help = param.help_;
			multiline |= help.size() > kTextWidth;
			while (help.size() > kTextWidth) {
				// Auto wrap lines wider than text width
				size_t space_idx = help.rfind(' ', kTextWidth);
				if (space_idx != std::string::npos) {
					std::cout << help.substr(0, space_idx) << std::endl << indent_string;
					help = help.substr(space_idx + 1);
				} else {
					break;
				}
			}
			std::cout << help << std::endl;
		}
	}

	std::cout << std::endl
	          << _("Bug reports? Suggestions? Check out the project website:\n"
	               "        https://www.widelands.org/\n\n"
	               "Hope you enjoy this game!")
	          << std::endl;
}
