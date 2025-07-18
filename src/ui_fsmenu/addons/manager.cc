/*
 * Copyright (C) 2020-2025 by the Widelands Development Team
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

#include "ui_fsmenu/addons/manager.h"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <memory>
#include <ostream>
#include <sstream>

#include "base/i18n.h"
#include "base/log.h"
#include "base/time_string.h"
#include "base/warning.h"
#include "graphic/graphic.h"
#include "graphic/image_cache.h"
#include "graphic/style_manager.h"
#include "graphic/text_layout.h"
#include "io/profile.h"
#include "logic/filesystem_constants.h"
#include "logic/game.h"
#include "scripting/lua_table.h"
#include "ui_basic/color_chooser.h"
#include "ui_basic/messagebox.h"
#include "ui_basic/textinput.h"
#include "ui_fsmenu/addons/contact.h"
#include "ui_fsmenu/addons/login_box.h"
#include "ui_fsmenu/addons/packager.h"
#include "ui_fsmenu/addons/progress.h"
#include "ui_fsmenu/addons/rows_ui.h"
#include "ui_fsmenu/addons/screenshot_upload.h"
#include "wlapplication.h"
#include "wlapplication_options.h"

namespace AddOnsUI {

constexpr const char* const kDocumentationURL = "https://www.widelands.org/documentation/add-ons/";
constexpr const char* const kForumURL = "https://www.widelands.org/forum/forum/17/";

// UI::Box by defaults limits its size to the window resolution. We use scrollbars,
// so we can and need to allow somewhat larger dimensions.
constexpr int32_t kHugeSize = std::numeric_limits<int32_t>::max() / 2;

std::string filesize_string(const uint32_t bytes) {
	if (bytes > 1000 * 1000 * 1000) {
		return format_l(_("%.2f GB"), (bytes / (1000.f * 1000.f * 1000.f)));
	}
	if (bytes > 1000 * 1000) {
		return format_l(_("%.2f MB"), (bytes / (1000.f * 1000.f)));
	}
	if (bytes > 1000) {
		return format_l(_("%.2f kB"), (bytes / 1000.f));
	}
	return format_l(_("%u bytes"), bytes);
}

std::string time_string(const std::time_t& time) {
	std::ostringstream oss("");
	try {
		oss.imbue(std::locale(i18n::get_locale()));
	} catch (...) {
		// silently ignore
	}
	oss << std::put_time(std::localtime(&time), "%c");
	return oss.str();
}

static inline std::function<bool(const std::shared_ptr<AddOns::AddOnInfo>,
                                 const std::shared_ptr<AddOns::AddOnInfo>)>
create_sort_functor(const AddOnSortingCriteria sort_by) {
	return [sort_by](const std::shared_ptr<AddOns::AddOnInfo> a,
	                 const std::shared_ptr<AddOns::AddOnInfo> b) {
		switch (sort_by) {
		case AddOnSortingCriteria::kNameABC:
			return a->descname().compare(b->descname()) < 0;
		case AddOnSortingCriteria::kNameCBA:
			return a->descname().compare(b->descname()) > 0;

		case AddOnSortingCriteria::kFewestDownloads:
			return a->download_count < b->download_count;
		case AddOnSortingCriteria::kMostDownloads:
			return a->download_count > b->download_count;

		case AddOnSortingCriteria::kOldest:
			return a->upload_timestamp < b->upload_timestamp;
		case AddOnSortingCriteria::kNewest:
			return a->upload_timestamp > b->upload_timestamp;

		case AddOnSortingCriteria::kLowestRating:
			if (a->number_of_votes() == 0) {
				// Add-ons without votes should always end up
				// below any others when sorting by rating
				return false;
			} else if (b->number_of_votes() == 0) {
				return true;
			} else if (std::abs(a->average_rating() - b->average_rating()) < 0.01) {
				// ambiguity – always choose the one with more votes
				return a->number_of_votes() > b->number_of_votes();
			} else {
				return a->average_rating() < b->average_rating();
			}
		case AddOnSortingCriteria::kHighestRating:
			if (a->number_of_votes() == 0) {
				return false;
			} else if (b->number_of_votes() == 0) {
				return true;
			} else if (std::abs(a->average_rating() - b->average_rating()) < 0.01) {
				return a->number_of_votes() > b->number_of_votes();
			} else {
				return a->average_rating() > b->average_rating();
			}

		default:
			NEVER_HERE();
		}
	};
}

const std::map<unsigned, std::function<AddOnQuality()>> AddOnQuality::kQualities = {  // NOLINT
   {0,
    []() {
	    return AddOnQuality(g_image_cache->get("images/ui_basic/different.png"),
	                        /** TRANSLATORS: This is an add-on code quality rating */
	                        pgettext("quality", "Any"), _("Quality not yet evaluated"));
    }},
   {1,
    []() {
	    return AddOnQuality(playercolor_image(RGBColor(0xcd7f32), "images/players/team.png"),
	                        /** TRANSLATORS: This is an add-on code quality rating */
	                        pgettext("quality", "Poor"),
	                        _("This add-on may cause major glitches and errors."));
    }},
   {2,
    []() {
	    return AddOnQuality(playercolor_image(RGBColor(0xC0C0C0), "images/players/team.png"),
	                        /** TRANSLATORS: This is an add-on code quality rating */
	                        pgettext("quality", "Good"), _("This add-on works as advertised."));
    }},
   {3, []() {
	    return AddOnQuality(playercolor_image(RGBColor(0xFFD700), "images/players/team.png"),
	                        /** TRANSLATORS: This is an add-on code quality rating */
	                        pgettext("quality", "Excellent"),
	                        _("This add-on has been decorated for its remarkably high quality."));
    }}};

struct OperationCancelledByUserException : std::exception {};

AddOnsCtrl::AddOnsCtrl(FsMenu::MainMenu& fsmm, UI::UniqueWindow::Registry& reg)
   : UI::UniqueWindow(&fsmm,
                      UI::WindowStyle::kFsMenu,
                      "addons",
                      &reg,
                      fsmm.calc_desired_window_width(UI::Window::WindowLayoutID::kFsMenuDefault),
                      fsmm.calc_desired_window_height(UI::Window::WindowLayoutID::kFsMenuDefault),
                      _("Add-On Manager")),
     fsmm_(fsmm),
     main_box_(this, UI::PanelStyle::kFsMenu, "main_box", 0, 0, UI::Box::Vertical),
     buttons_box_(&main_box_, UI::PanelStyle::kFsMenu, "buttons_box", 0, 0, UI::Box::Horizontal),
     warn_requirements_(&main_box_,
                        "warn_requirements",
                        0,
                        0,
                        get_w(),
                        get_h() / 12,
                        UI::PanelStyle::kFsMenu,
                        "",
                        UI::Align::kCenter),
     tabs_placeholder_(&main_box_, UI::PanelStyle::kFsMenu, "tabs_placeholder", 0, 0, 0, 0),
     tabs_(this, UI::TabPanelStyle::kFsMenu, "tabs"),
     installed_addons_outer_wrapper_(
        &tabs_, UI::PanelStyle::kFsMenu, "installed_outer_wrapper_box", 0, 0, UI::Box::Horizontal),
     installed_addons_inner_wrapper_(&installed_addons_outer_wrapper_,
                                     UI::PanelStyle::kFsMenu,
                                     "installed_inner_wrapper_box",
                                     0,
                                     0,
                                     UI::Box::Vertical),
     installed_addons_buttons_box_(&installed_addons_outer_wrapper_,
                                   UI::PanelStyle::kFsMenu,
                                   "installed_buttons_box",
                                   0,
                                   0,
                                   UI::Box::Vertical),
     installed_addons_box_(&installed_addons_inner_wrapper_,
                           UI::PanelStyle::kFsMenu,
                           "installed_box",
                           0,
                           0,
                           UI::Box::Vertical,
                           kHugeSize,
                           kHugeSize),
     browse_addons_outer_wrapper_(
        &tabs_, UI::PanelStyle::kFsMenu, "browse_outer_wrapper_box", 0, 0, UI::Box::Vertical),
     browse_addons_inner_wrapper_(&browse_addons_outer_wrapper_,
                                  UI::PanelStyle::kFsMenu,
                                  "browse_inner_wrapper_box",
                                  0,
                                  0,
                                  UI::Box::Vertical),
     browse_addons_buttons_box_(&browse_addons_outer_wrapper_,
                                UI::PanelStyle::kFsMenu,
                                "browse_buttons_box",
                                0,
                                0,
                                UI::Box::Horizontal),
     browse_addons_buttons_box_lvbox_(&browse_addons_buttons_box_,
                                      UI::PanelStyle::kFsMenu,
                                      "browse_buttons_left_vbox",
                                      0,
                                      0,
                                      UI::Box::Vertical),
     browse_addons_buttons_box_rvbox_(&browse_addons_buttons_box_,
                                      UI::PanelStyle::kFsMenu,
                                      "browse_buttons_right_vbox",
                                      0,
                                      0,
                                      UI::Box::Vertical),
     browse_addons_buttons_box_category_box_(&browse_addons_buttons_box_lvbox_,
                                             UI::PanelStyle::kFsMenu,
                                             "browse_buttons_category_box",
                                             0,
                                             0,
                                             UI::Box::Horizontal),
     browse_addons_buttons_box_right_hbox_(&browse_addons_buttons_box_rvbox_,
                                           UI::PanelStyle::kFsMenu,
                                           "browse_buttons_hbox",
                                           0,
                                           0,
                                           UI::Box::Horizontal),
     browse_addons_box_(&browse_addons_inner_wrapper_,
                        UI::PanelStyle::kFsMenu,
                        "browse_box",
                        0,
                        0,
                        UI::Box::Vertical,
                        kHugeSize,
                        kHugeSize),
     maps_outer_wrapper_(
        &tabs_, UI::PanelStyle::kFsMenu, "maps_outer_wrapper_box", 0, 0, UI::Box::Vertical),
     maps_inner_wrapper_(&maps_outer_wrapper_,
                         UI::PanelStyle::kFsMenu,
                         "maps_inner_wrapper_box",
                         0,
                         0,
                         UI::Box::Vertical),
     maps_buttons_box_(&maps_outer_wrapper_,
                       UI::PanelStyle::kFsMenu,
                       "maps_buttons_box",
                       0,
                       0,
                       UI::Box::Horizontal),
     maps_box_(&maps_inner_wrapper_,
               UI::PanelStyle::kFsMenu,
               "maps_box",
               0,
               0,
               UI::Box::Vertical,
               kHugeSize,
               kHugeSize),
     filter_maps_lvbox_(
        &maps_buttons_box_, UI::PanelStyle::kFsMenu, "maps_buttons_lvbox", 0, 0, UI::Box::Vertical),
     filter_maps_rvbox_min_(&maps_buttons_box_,
                            UI::PanelStyle::kFsMenu,
                            "maps_buttons_rvbox_min",
                            0,
                            0,
                            UI::Box::Vertical),
     filter_maps_rvbox_max_(&maps_buttons_box_,
                            UI::PanelStyle::kFsMenu,
                            "maps_buttons_rvbox_max",
                            0,
                            0,
                            UI::Box::Vertical),
     filter_maps_lhbox_(&filter_maps_lvbox_,
                        UI::PanelStyle::kFsMenu,
                        "maps_buttons_lhbox",
                        0,
                        0,
                        UI::Box::Horizontal),
     dev_box_(&tabs_, UI::PanelStyle::kFsMenu, "development_box", 0, 0, UI::Box::Vertical),
     filter_browse_name_(&browse_addons_buttons_box_rvbox_,
                         "filter_browse_name",
                         0,
                         0,
                         100,
                         UI::PanelStyle::kFsMenu),
     filter_maps_name_(&filter_maps_lvbox_, "filter_maps_name", 0, 0, 100, UI::PanelStyle::kFsMenu),
     filter_browse_verified_(&browse_addons_buttons_box_right_hbox_,
                             UI::PanelStyle::kFsMenu,
                             "filter_browse_verified",
                             Vector2i(0, 0),
                             _("Verified only"),
                             _("Show only verified add-ons")),
     sort_order_browse_(&browse_addons_buttons_box_rvbox_,
                        "sort_browse",
                        0,
                        0,
                        0,
                        10,
                        filter_browse_name_.get_h(),
                        _("Sort by"),
                        UI::DropdownType::kTextual,
                        UI::PanelStyle::kFsMenu,
                        UI::ButtonStyle::kFsMenuSecondary),
     sort_order_maps_(&filter_maps_lvbox_,
                      "sort_maps",
                      0,
                      0,
                      0,
                      10,
                      filter_maps_name_.get_h(),
                      _("Sort by"),
                      UI::DropdownType::kTextual,
                      UI::PanelStyle::kFsMenu,
                      UI::ButtonStyle::kFsMenuSecondary),
     filter_browse_quality_(&browse_addons_buttons_box_right_hbox_,
                            "quality",
                            0,
                            0,
                            0,
                            10,
                            filter_browse_name_.get_h(),
                            _("Minimum quality"),
                            UI::DropdownType::kTextual,
                            UI::PanelStyle::kFsMenu,
                            UI::ButtonStyle::kFsMenuSecondary),
     filter_maps_min_players_(&filter_maps_rvbox_min_,
                              "filter_maps_min_players",
                              0,
                              0,
                              0,
                              150,
                              1,
                              1,
                              kMaxPlayers,
                              UI::PanelStyle::kFsMenu,
                              _("Min Players:"),
                              UI::SpinBox::Units::kNone,
                              UI::SpinBox::Type::kSmall),
     filter_maps_min_w_(&filter_maps_rvbox_min_,
                        "filter_maps_min_w",
                        0,
                        0,
                        0,
                        150,
                        0,
                        0,
                        0,
                        UI::PanelStyle::kFsMenu,
                        _("Min Width:"),
                        UI::SpinBox::Units::kNone,
                        UI::SpinBox::Type::kValueList),
     filter_maps_min_h_(&filter_maps_rvbox_min_,
                        "filter_maps_min_h",
                        0,
                        0,
                        0,
                        150,
                        0,
                        0,
                        0,
                        UI::PanelStyle::kFsMenu,
                        _("Min Height:"),
                        UI::SpinBox::Units::kNone,
                        UI::SpinBox::Type::kValueList),
     filter_maps_min_size_(&filter_maps_rvbox_min_,
                           "filter_maps_min_size",
                           0,
                           0,
                           0,
                           150,
                           0,
                           0,
                           0,
                           UI::PanelStyle::kFsMenu,
                           _("Min Size:"),
                           UI::SpinBox::Units::kNone,
                           UI::SpinBox::Type::kValueList),
     filter_maps_max_players_(&filter_maps_rvbox_max_,
                              "filter_maps_max_players",
                              0,
                              0,
                              0,
                              150,
                              kMaxPlayers,
                              1,
                              kMaxPlayers,
                              UI::PanelStyle::kFsMenu,
                              _("Max Players:"),
                              UI::SpinBox::Units::kNone,
                              UI::SpinBox::Type::kSmall),
     filter_maps_max_w_(&filter_maps_rvbox_max_,
                        "filter_maps_max_w",
                        0,
                        0,
                        0,
                        150,
                        0,
                        0,
                        0,
                        UI::PanelStyle::kFsMenu,
                        _("Max Width:"),
                        UI::SpinBox::Units::kNone,
                        UI::SpinBox::Type::kValueList),
     filter_maps_max_h_(&filter_maps_rvbox_max_,
                        "filter_maps_max_h",
                        0,
                        0,
                        0,
                        150,
                        0,
                        0,
                        0,
                        UI::PanelStyle::kFsMenu,
                        _("Max Height:"),
                        UI::SpinBox::Units::kNone,
                        UI::SpinBox::Type::kValueList),
     filter_maps_max_size_(&filter_maps_rvbox_max_,
                           "filter_maps_max_size",
                           0,
                           0,
                           0,
                           150,
                           0,
                           0,
                           0,
                           UI::PanelStyle::kFsMenu,
                           _("Max Size:"),
                           UI::SpinBox::Units::kNone,
                           UI::SpinBox::Type::kValueList),
     upload_addon_(&dev_box_,
                   "upload_addon",
                   0,
                   0,
                   get_inner_w() / 2,
                   8,
                   kRowButtonSize,
                   _("Choose add-on to upload…"),
                   UI::DropdownType::kTextualMenu,
                   UI::PanelStyle::kFsMenu,
                   UI::ButtonStyle::kFsMenuSecondary),
     upload_screenshot_(&dev_box_,
                        "upload_screenie",
                        0,
                        0,
                        get_inner_w() / 2,
                        8,
                        kRowButtonSize,
                        _("Upload a screenshot…"),
                        UI::DropdownType::kTextualMenu,
                        UI::PanelStyle::kFsMenu,
                        UI::ButtonStyle::kFsMenuSecondary),
     upload_addon_accept_(&dev_box_,
                          UI::PanelStyle::kFsMenu,
                          "upload_addon_accept",
                          Vector2i(0, 0),
                          _("Understood and confirmed"),
                          _("By ticking this checkbox, you confirm that you have read and agree to "
                            "the above terms.")),
     filter_browse_reset_(&browse_addons_buttons_box_lvbox_,
                          "f_browse_reset",
                          0,
                          0,
                          24,
                          24,
                          UI::ButtonStyle::kFsMenuSecondary,
                          _("Reset"),
                          _("Reset the filters")),
     filter_maps_reset_(&filter_maps_lvbox_,
                        "f_maps_reset",
                        0,
                        0,
                        24,
                        24,
                        UI::ButtonStyle::kFsMenuSecondary,
                        _("Reset"),
                        _("Reset the filters")),
     upgrade_all_(&buttons_box_,
                  "upgrade_all",
                  0,
                  0,
                  kRowButtonSize,
                  kRowButtonSize,
                  UI::ButtonStyle::kFsMenuSecondary,
                  ""),
     refresh_(&buttons_box_,
              "refresh",
              0,
              0,
              kRowButtonSize,
              kRowButtonSize,
              UI::ButtonStyle::kFsMenuSecondary,
              _("Refresh"),
              _("Refresh the list of add-ons available from the server")),
     ok_(&buttons_box_,
         "ok",
         0,
         0,
         kRowButtonSize,
         kRowButtonSize,
         UI::ButtonStyle::kFsMenuPrimary,
         _("OK")),
#if 0  // TODO(Nordfriese): Disabled autofix_dependencies for v1.0
     autofix_dependencies_(&buttons_box_,
                           "autofix",
                           0,
                           0,
                           kRowButtonSize,
                           kRowButtonSize,
                           UI::ButtonStyle::kFsMenuSecondary,
                           _("Fix dependencies…"),
                           _("Try to automatically fix the dependency errors")),
#endif
     move_top_(&installed_addons_buttons_box_,
               "move_top",
               0,
               0,
               kRowButtonSize,
               kRowButtonSize,
               UI::ButtonStyle::kFsMenuSecondary,
               g_image_cache->get("images/ui_basic/scrollbar_up_fast.png"),
               _("Move selected add-on to top")),
     move_up_(&installed_addons_buttons_box_,
              "move_up",
              0,
              0,
              kRowButtonSize,
              kRowButtonSize,
              UI::ButtonStyle::kFsMenuSecondary,
              g_image_cache->get("images/ui_basic/scrollbar_up.png"),
              _("Move selected add-on one step up")),
     move_down_(&installed_addons_buttons_box_,
                "move_down",
                0,
                0,
                kRowButtonSize,
                kRowButtonSize,
                UI::ButtonStyle::kFsMenuSecondary,
                g_image_cache->get("images/ui_basic/scrollbar_down.png"),
                _("Move selected add-on one step down")),
     move_bottom_(&installed_addons_buttons_box_,
                  "move_bottom",
                  0,
                  0,
                  kRowButtonSize,
                  kRowButtonSize,
                  UI::ButtonStyle::kFsMenuSecondary,
                  g_image_cache->get("images/ui_basic/scrollbar_down_fast.png"),
                  _("Move selected add-on to bottom")),
     launch_packager_(&dev_box_,
                      "packager",
                      0,
                      0,
                      0,
                      0,
                      UI::ButtonStyle::kFsMenuSecondary,
                      _("Launch the add-ons packager…")),
     login_button_(this, "login", 0, 0, 0, 0, UI::ButtonStyle::kFsMenuSecondary, ""),
     contact_(&dev_box_,
              "contact",
              0,
              0,
              0,
              0,
              UI::ButtonStyle::kFsMenuSecondary,
              /** TRANSLATORS: This button allows the user to send a message to the Widelands
                 Development Team */
              _("Contact us…")),
     server_name_(this,
                  UI::PanelStyle::kFsMenu,
                  "server_name",
                  UI::FontStyle::kWarning,
                  0,
                  0,
                  0,
                  0,
                  "",
                  UI::Align::kRight) {

	{
		Profile prof(kAddOnLocaleVersions.c_str());
		current_i18n_version_ = prof.pull_section("global").get_natural("websitemaps", 0);
	}

	installed_addons_box_.set_flag(UI::Panel::pf_unlimited_size, true);
	browse_addons_box_.set_flag(UI::Panel::pf_unlimited_size, true);
	maps_box_.set_flag(UI::Panel::pf_unlimited_size, true);

	dev_box_.set_force_scrolling(true);
	dev_box_.add(new UI::Textarea(&dev_box_, UI::PanelStyle::kFsMenu, "label_development",
	                              UI::FontStyle::kFsMenuInfoPanelHeading,
	                              _("Tools for Add-Ons Developers"), UI::Align::kCenter),
	             UI::Box::Resizing::kFullSize);
	dev_box_.add_space(kRowButtonSize);
	{
		UI::MultilineTextarea* m = new UI::MultilineTextarea(
		   &dev_box_, "message_development", 0, 0, 100, 100, UI::PanelStyle::kFsMenu, "",
		   UI::Align::kLeft, UI::MultilineTextarea::ScrollMode::kNoScrolling);
		m->set_style(UI::FontStyle::kFsMenuInfoPanelParagraph);
		m->set_text(_("The interactive add-ons packager allows you to create, edit, and delete "
		              "add-ons. You can bundle maps designed with the Widelands Map Editor as an "
		              "add-on using the graphical interface and share them with other players, "
		              "without having to write a single line of code."));
		dev_box_.add(m, UI::Box::Resizing::kFullSize);
	}
	dev_box_.add_space(kRowButtonSpacing);
	dev_box_.add(&launch_packager_);

	dev_box_.add_space(kRowButtonSize);
	dev_box_.add(new UI::MultilineTextarea(
	                &dev_box_, "message_links", 0, 0, 100, 100, UI::PanelStyle::kFsMenu,
	                format("<rt><p>%1$s</p></rt>",
	                       g_style_manager->font_style(UI::FontStyle::kFsMenuInfoPanelParagraph)
	                          .as_font_tag(format(
	                             _("For more information regarding how to develop and package your "
	                               "own add-ons, please visit %s."),
	                             g_style_manager->font_style(UI::FontStyle::kFsTooltip)
	                                .as_font_tag(as_url_hyperlink(kDocumentationURL))))),
	                UI::Align::kLeft, UI::MultilineTextarea::ScrollMode::kNoScrolling),
	             UI::Box::Resizing::kFullSize);
	dev_box_.add_space(kRowButtonSpacing);

	dev_box_.add_space(kRowButtonSize);
	dev_box_.add(
	   new UI::MultilineTextarea(
	      &dev_box_, "message_detailed_info", 0, 0, 100, 100, UI::PanelStyle::kFsMenu,
	      format(
	         "<rt><p>%s</p><p>%s</p><p>%s</p><p>%s</p></rt>",
	         g_style_manager->font_style(UI::FontStyle::kFsMenuInfoPanelParagraph)
	            .as_font_tag(format(
	               _("Here, you can upload your add-ons to the server to make them available to "
	                 "other players. By uploading, you agree to publish your creation under the "
	                 "terms of the GNU General Public License (GPL) version 2 or any later version "
	                 "(the same license under which Widelands itself is distributed). For more "
	                 "information on the GPL, please refer to ‘About Widelands’ → ‘License’ in "
	                 "the main menu or see %s."),
	               as_url_hyperlink("https://www.gnu.org/licenses/"))),
	         g_style_manager->font_style(UI::FontStyle::kFsMenuInfoPanelParagraph)
	            .as_font_tag(
	               _("It is forbidden to upload add-ons containing harmful or malicious content or "
	                 "spam. By uploading an add-on, you assert that the add-on is of your own "
	                 "creation "
	                 "or you have the add-on’s author(s) permission to submit it in their stead.")),
	         g_style_manager->font_style(UI::FontStyle::kFsMenuInfoPanelParagraph)
	            .as_font_tag(_(
	               "You are required to have read the add-ons documentation under the link given "
	               "further above before submitting content. Since the documentation is subject to "
	               "frequent changes, ensure that you have read it recently and that you followed "
	               "all guidelines stated there.")),
	         g_style_manager->font_style(UI::FontStyle::kFsMenuInfoPanelParagraph)
	            .as_font_tag(
	               _("The Widelands Development Team will review your add-on soon after "
	                 "uploading. In case they have further inquiries, they will contact you "
	                 "via a PM on the Widelands website; therefore please check the inbox "
	                 "of your online user profile page frequently."))),
	      UI::Align::kLeft, UI::MultilineTextarea::ScrollMode::kNoScrolling),
	   UI::Box::Resizing::kFullSize);

	dev_box_.add_space(kRowButtonSpacing);
	dev_box_.add(&upload_addon_accept_);
	dev_box_.add_space(kRowButtonSpacing);
	dev_box_.add(&upload_addon_);
	dev_box_.add_space(kRowButtonSpacing);
	dev_box_.add(&upload_screenshot_);

	upload_addon_accept_.changed.connect([this]() { update_login_button(nullptr); });
	upload_addon_.selected.connect([this]() { upload_addon(upload_addon_.get_selected()); });
	upload_screenshot_.selected.connect([this]() {
		upload_screenshot_.set_list_visibility(false);
		std::shared_ptr<AddOns::AddOnInfo> info = upload_screenshot_.get_selected();
		ScreenshotUploadWindow s(*this, info, find_remote(info->internal_name));
		s.run<UI::Panel::Returncodes>();
	});

	dev_box_.add_space(kRowButtonSize);
	dev_box_.add(
	   new UI::MultilineTextarea(
	      &dev_box_, "message_contact_public", 0, 0, 100, 100, UI::PanelStyle::kFsMenu,
	      format("<rt><p>%1$s</p></rt>",
	             g_style_manager->font_style(UI::FontStyle::kFsMenuInfoPanelParagraph)
	                .as_font_tag(
	                   format(_("Technical problems? Unclear documentation? Advanced needs such "
	                            "as deletion of an add-on or collaborating with another add-on "
	                            "designer? Please visit our forums at %s, explain your needs, and "
	                            "the Widelands Development Team will be happy to help."),
	                          g_style_manager->font_style(UI::FontStyle::kFsTooltip)
	                             .as_font_tag(as_url_hyperlink(kForumURL))))),
	      UI::Align::kLeft, UI::MultilineTextarea::ScrollMode::kNoScrolling),
	   UI::Box::Resizing::kFullSize);
	dev_box_.add_space(kRowButtonSpacing);

	dev_box_.add_space(kRowButtonSize);
	dev_box_.add(
	   new UI::MultilineTextarea(
	      &dev_box_, "message_contact_private", 0, 0, 100, 100, UI::PanelStyle::kFsMenu,
	      format("<rt><p>%1$s</p></rt>",
	             g_style_manager->font_style(UI::FontStyle::kFsMenuInfoPanelParagraph)
	                .as_font_tag(
	                   _("Alternatively you can also send a message to the Widelands Development "
	                     "Team and we’ll try to help you with your enquiry as soon as we can. "
	                     "Only the server administrators can read messages sent in this way. "
	                     "Since other add-on designers can usually benefit from others’ questions "
	                     "and answers, please use this way of communication only if there is a "
	                     "reason why your concern is not for everyone’s ears. You’ll receive our "
	                     "reply via a PM on your Widelands website user profile page."))),
	      UI::Align::kLeft, UI::MultilineTextarea::ScrollMode::kNoScrolling),
	   UI::Box::Resizing::kFullSize);
	contact_.sigclicked.connect([this]() {
		ContactForm c(*this);
		c.run<UI::Panel::Returncodes>();
	});
	dev_box_.add_space(kRowButtonSpacing);
	dev_box_.add(&contact_);

	installed_addons_buttons_box_.add(&move_top_, UI::Box::Resizing::kAlign, UI::Align::kCenter);
	installed_addons_buttons_box_.add_space(kRowButtonSpacing);
	installed_addons_buttons_box_.add(&move_up_, UI::Box::Resizing::kAlign, UI::Align::kCenter);
	installed_addons_buttons_box_.add_space(kRowButtonSize + 2 * kRowButtonSpacing);
	installed_addons_buttons_box_.add(&move_down_, UI::Box::Resizing::kAlign, UI::Align::kCenter);
	installed_addons_buttons_box_.add_space(kRowButtonSpacing);
	installed_addons_buttons_box_.add(&move_bottom_, UI::Box::Resizing::kAlign, UI::Align::kCenter);
	installed_addons_outer_wrapper_.add(
	   &installed_addons_inner_wrapper_, UI::Box::Resizing::kExpandBoth);
	installed_addons_outer_wrapper_.add_space(kRowButtonSpacing);
	installed_addons_outer_wrapper_.add(
	   &installed_addons_buttons_box_, UI::Box::Resizing::kAlign, UI::Align::kCenter);

	browse_addons_outer_wrapper_.add(&browse_addons_buttons_box_, UI::Box::Resizing::kFullSize);
	browse_addons_outer_wrapper_.add_space(2 * kRowButtonSpacing);
	browse_addons_outer_wrapper_.add(&browse_addons_inner_wrapper_, UI::Box::Resizing::kExpandBoth);

	maps_outer_wrapper_.add(&maps_buttons_box_, UI::Box::Resizing::kFullSize);
	maps_outer_wrapper_.add_space(2 * kRowButtonSpacing);
	maps_outer_wrapper_.add(&maps_inner_wrapper_, UI::Box::Resizing::kExpandBoth);

	installed_addons_inner_wrapper_.add(&installed_addons_box_, UI::Box::Resizing::kExpandBoth);
	browse_addons_inner_wrapper_.add(&browse_addons_box_, UI::Box::Resizing::kExpandBoth);
	maps_inner_wrapper_.add(&maps_box_, UI::Box::Resizing::kExpandBoth);
	tabs_.add("my", "", &installed_addons_outer_wrapper_, _("Manage your installed add-ons"));
	tabs_.add("all", "", &browse_addons_outer_wrapper_,
	          _("Browse and install add-ons available on the server"));
	tabs_.add("maps", "", &maps_outer_wrapper_,
	          format_l(_("Browse and install maps from the widelands.org website maps archive. Like "
	                     "manually downloaded maps, installed website maps are stored in the ‘%s’ "
	                     "directory under the Widelands home directory."),
	                   kDownloadedMapsDirFull));
	tabs_.add("development", _("Development"), &dev_box_, _("Tools for add-on developers"));

	for (auto* so : {&sort_order_browse_, &sort_order_maps_}) {
		/** TRANSLATORS: Sort add-ons alphabetically by name */
		so->add(_("Name"), AddOnSortingCriteria::kNameABC);
		/** TRANSLATORS: Sort add-ons alphabetically by name (inverted) */
		so->add(_("Name (descending)"), AddOnSortingCriteria::kNameCBA);
		/** TRANSLATORS: Sort add-ons by average rating */
		so->add(_("Most popular"), AddOnSortingCriteria::kHighestRating, nullptr, true);
		/** TRANSLATORS: Sort add-ons by average rating */
		so->add(_("Least popular"), AddOnSortingCriteria::kLowestRating);
		/** TRANSLATORS: Sort add-ons by how often they were downloaded */
		so->add(_("Most often downloaded"), AddOnSortingCriteria::kMostDownloads);
		/** TRANSLATORS: Sort add-ons by how often they were downloaded */
		so->add(_("Least often downloaded"), AddOnSortingCriteria::kFewestDownloads);
		/** TRANSLATORS: Sort add-ons by upload date/time */
		so->add(_("Oldest"), AddOnSortingCriteria::kOldest);
		/** TRANSLATORS: Sort add-ons by upload date/time */
		so->add(_("Newest"), AddOnSortingCriteria::kNewest);
	}

	for (const auto& pair : AddOnQuality::kQualities) {
		const AddOnQuality q = pair.second();
		filter_browse_quality_.add(q.name, pair.first, q.icon, pair.first == 2, q.description);
	}

	filter_browse_verified_.set_state(true);
	filter_browse_name_.set_tooltip(_("Filter add-ons by name"));
	filter_maps_name_.set_tooltip(_("Filter maps by name"));
	{
		uint8_t index = 0;
		for (const auto& pair : AddOns::kAddOnCategories) {
			if (pair.second.hide) {
				continue;
			}
			UI::Checkbox* c =
			   new UI::Checkbox(&browse_addons_buttons_box_category_box_, UI::PanelStyle::kFsMenu,
			                    format("category_%s", pair.second.internal_name), Vector2i(0, 0),
			                    g_image_cache->get(pair.second.icon),
			                    format(_("Toggle category ‘%s’"), pair.second.descname()));
			filter_browse_category_[pair.first] = c;
			c->set_state(true);
			c->changed.connect([this, &pair]() { category_filter_browse_changed(pair.first); });
			c->set_desired_size(kRowButtonSize, kRowButtonSize);
			if (index > 0) {
				browse_addons_buttons_box_category_box_.add_space(kRowButtonSpacing);
			}
			browse_addons_buttons_box_category_box_.add(
			   c, UI::Box::Resizing::kAlign, UI::Align::kCenter);
			++index;
		}
	}

	{
		std::vector<Widelands::Map::OldWorldInfo> worlds = {
		   {"", "", "world/pics/one_world.png", []() { return _("One World"); }}};
		worlds.insert(worlds.end(), Widelands::Map::kOldWorldNames.begin(),
		              Widelands::Map::kOldWorldNames.end());
		uint8_t index = 0;
		for (const Widelands::Map::OldWorldInfo& world : worlds) {
			UI::Checkbox* c = new UI::Checkbox(&filter_maps_lhbox_, UI::PanelStyle::kFsMenu,
			                                   format("world_%s", world.old_name), Vector2i(0, 0),
			                                   g_image_cache->get(world.icon),
			                                   format(_("Toggle world ‘%s’"), world.descname()));
			filter_maps_world_[world.old_name] = c;
			c->set_state(true);
			c->changed.connect([this, world]() { world_filter_maps_changed(world.old_name); });
			c->set_desired_size(kRowButtonSize, kRowButtonSize);
			if (index > 0) {
				filter_maps_lhbox_.add_space(kRowButtonSpacing);
			}
			filter_maps_lhbox_.add(c, UI::Box::Resizing::kAlign, UI::Align::kCenter);
			++index;
		}
	}

	browse_addons_buttons_box_right_hbox_.add(
	   &filter_browse_verified_, UI::Box::Resizing::kAlign, UI::Align::kCenter);
	browse_addons_buttons_box_right_hbox_.add(
	   &filter_browse_quality_, UI::Box::Resizing::kExpandBoth);

	browse_addons_buttons_box_rvbox_.add(
	   &browse_addons_buttons_box_right_hbox_, UI::Box::Resizing::kFullSize);
	browse_addons_buttons_box_rvbox_.add_space(kRowButtonSpacing);
	browse_addons_buttons_box_rvbox_.add(&filter_browse_name_, UI::Box::Resizing::kFullSize);
	browse_addons_buttons_box_rvbox_.add_space(kRowButtonSpacing);
	browse_addons_buttons_box_rvbox_.add(&sort_order_browse_, UI::Box::Resizing::kFullSize);

	browse_addons_buttons_box_lvbox_.add(
	   &browse_addons_buttons_box_category_box_, UI::Box::Resizing::kExpandBoth);
	browse_addons_buttons_box_lvbox_.add_inf_space();
	browse_addons_buttons_box_lvbox_.add(&filter_browse_reset_, UI::Box::Resizing::kFullSize);

	browse_addons_buttons_box_.add(&browse_addons_buttons_box_lvbox_, UI::Box::Resizing::kFullSize);
	browse_addons_buttons_box_.add_space(kRowButtonSpacing);
	browse_addons_buttons_box_.add(
	   &browse_addons_buttons_box_rvbox_, UI::Box::Resizing::kExpandBoth);

	filter_maps_lvbox_.add(&sort_order_maps_, UI::Box::Resizing::kFullSize);
	filter_maps_lvbox_.add_space(kRowButtonSpacing);
	filter_maps_lvbox_.add(&filter_maps_lhbox_, UI::Box::Resizing::kAlign, UI::Align::kCenter);
	filter_maps_lvbox_.add_space(kRowButtonSpacing);
	filter_maps_lvbox_.add(&filter_maps_name_, UI::Box::Resizing::kFullSize);
	filter_maps_lvbox_.add_space(kRowButtonSpacing);
	filter_maps_lvbox_.add(&filter_maps_reset_, UI::Box::Resizing::kFullSize);

	// Spinboxes and their width requirements...
	filter_maps_rvbox_min_.set_size(300, 100);
	filter_maps_rvbox_max_.set_size(300, 100);
	filter_maps_rvbox_min_.add(&filter_maps_min_players_, UI::Box::Resizing::kFullSize);
	filter_maps_rvbox_min_.add_space(kRowButtonSpacing);
	filter_maps_rvbox_min_.add(&filter_maps_min_w_, UI::Box::Resizing::kFullSize);
	filter_maps_rvbox_min_.add_space(kRowButtonSpacing);
	filter_maps_rvbox_min_.add(&filter_maps_min_h_, UI::Box::Resizing::kFullSize);
	filter_maps_rvbox_min_.add_space(kRowButtonSpacing);
	filter_maps_rvbox_min_.add(&filter_maps_min_size_, UI::Box::Resizing::kFullSize);

	filter_maps_rvbox_max_.add(&filter_maps_max_players_, UI::Box::Resizing::kFullSize);
	filter_maps_rvbox_max_.add_space(kRowButtonSpacing);
	filter_maps_rvbox_max_.add(&filter_maps_max_w_, UI::Box::Resizing::kFullSize);
	filter_maps_rvbox_max_.add_space(kRowButtonSpacing);
	filter_maps_rvbox_max_.add(&filter_maps_max_h_, UI::Box::Resizing::kFullSize);
	filter_maps_rvbox_max_.add_space(kRowButtonSpacing);
	filter_maps_rvbox_max_.add(&filter_maps_max_size_, UI::Box::Resizing::kFullSize);

	maps_buttons_box_.add(&filter_maps_lvbox_, UI::Box::Resizing::kExpandBoth);
	maps_buttons_box_.add_space(kRowButtonSpacing);
	maps_buttons_box_.add(&filter_maps_rvbox_min_, UI::Box::Resizing::kFullSize);
	maps_buttons_box_.add_space(kRowButtonSpacing);
	maps_buttons_box_.add(&filter_maps_rvbox_max_, UI::Box::Resizing::kFullSize);

	filter_maps_min_w_.set_value_list(Widelands::kMapDimensions);
	filter_maps_min_h_.set_value_list(Widelands::kMapDimensions);
	filter_maps_max_w_.set_value_list(Widelands::kMapDimensions);
	filter_maps_max_h_.set_value_list(Widelands::kMapDimensions);
	filter_maps_min_size_.set_value_list(Widelands::Map::kMapFieldCounts);
	filter_maps_max_size_.set_value_list(Widelands::Map::kMapFieldCounts);
	filter_maps_max_w_.set_value(Widelands::kMapDimensions.size() - 1, false);
	filter_maps_max_h_.set_value(Widelands::kMapDimensions.size() - 1, false);
	filter_maps_max_size_.set_value(Widelands::Map::kMapFieldCounts.size() - 1, false);

	filter_browse_reset_.set_enabled(false);
	filter_maps_reset_.set_enabled(false);
	filter_browse_name_.changed.connect([this]() {
		filter_browse_reset_.set_enabled(true);
		rebuild_browse();
	});
	filter_maps_name_.changed.connect([this]() {
		filter_maps_reset_.set_enabled(true);
		rebuild_maps();
	});
	filter_browse_verified_.changed.connect([this]() {
		filter_browse_reset_.set_enabled(true);
		rebuild_browse();
	});
	sort_order_browse_.selected.connect([this]() { rebuild_browse(); });
	sort_order_maps_.selected.connect([this]() { rebuild_maps(); });
	filter_browse_quality_.selected.connect([this]() {
		filter_browse_reset_.set_enabled(true);
		rebuild_browse();
	});

	filter_maps_min_players_.changed.connect([this]() {
		filter_maps_reset_.set_enabled(true);
		rebuild_maps();
	});
	filter_maps_min_w_.changed.connect([this]() {
		filter_maps_reset_.set_enabled(true);
		rebuild_maps();
	});
	filter_maps_min_h_.changed.connect([this]() {
		filter_maps_reset_.set_enabled(true);
		rebuild_maps();
	});
	filter_maps_min_size_.changed.connect([this]() {
		filter_maps_reset_.set_enabled(true);
		rebuild_maps();
	});
	filter_maps_max_players_.changed.connect([this]() {
		filter_maps_reset_.set_enabled(true);
		rebuild_maps();
	});
	filter_maps_max_w_.changed.connect([this]() {
		filter_maps_reset_.set_enabled(true);
		rebuild_maps();
	});
	filter_maps_max_h_.changed.connect([this]() {
		filter_maps_reset_.set_enabled(true);
		rebuild_maps();
	});
	filter_maps_max_size_.changed.connect([this]() {
		filter_maps_reset_.set_enabled(true);
		rebuild_maps();
	});

	ok_.sigclicked.connect([this]() { die(); });
	refresh_.sigclicked.connect([this]() {
		refresh_remotes(matches_keymod(SDL_GetModState(), KMOD_CTRL));
		if (tabs_.active() != 2) {
			tabs_.activate(1);
		}
	});
	tabs_.sigclicked.connect([this]() {
		if ((tabs_.active() == 1 || tabs_.active() == 2) && remotes_.size() <= 1) {
			refresh_remotes(false);
		}
	});
#if 0  // TODO(Nordfriese): Disabled autofix_dependencies for v1.0
	autofix_dependencies_.sigclicked.connect([this]() { autofix_dependencies(); });
#endif

	filter_browse_reset_.sigclicked.connect([this]() {
		filter_browse_name_.set_text("");
		filter_browse_verified_.set_state(true, false);
		filter_browse_quality_.select(2);
		for (auto& pair : filter_browse_category_) {
			pair.second->set_state(true, false);
		}
		rebuild_browse();
		filter_browse_reset_.set_enabled(false);
	});
	filter_maps_reset_.sigclicked.connect([this]() {
		filter_maps_name_.set_text("");
		filter_maps_min_players_.set_value(1, false);
		filter_maps_min_w_.set_value(0, false);
		filter_maps_min_h_.set_value(0, false);
		filter_maps_min_size_.set_value(0, false);
		filter_maps_max_players_.set_value(kMaxPlayers, false);
		filter_maps_max_w_.set_value(Widelands::kMapDimensions.size() - 1, false);
		filter_maps_max_h_.set_value(Widelands::kMapDimensions.size() - 1, false);
		filter_maps_max_size_.set_value(Widelands::Map::kMapFieldCounts.size() - 1, false);
		for (auto& pair : filter_maps_world_) {
			pair.second->set_state(true, false);
		}
		rebuild_maps();
		filter_maps_reset_.set_enabled(false);
	});
	upgrade_all_.sigclicked.connect([this]() {
		std::vector<std::pair<std::shared_ptr<AddOns::AddOnInfo>, bool /* full upgrade */>> upgrades;
		bool all_verified = true;
		size_t nr_full_updates = 0;
		for (const auto& pair : cached_browse_rows_) {
			if (pair.second->is_visible() && pair.second->upgradeable()) {
				const bool full_upgrade = pair.second->full_upgrade_possible();
				upgrades.emplace_back(pair.second->info(), full_upgrade);
				if (full_upgrade) {
					all_verified &= pair.second->info()->verified;
					++nr_full_updates;
				}
			}
		}
		if (nr_full_updates > 0 && (!all_verified || !matches_keymod(SDL_GetModState(), KMOD_CTRL))) {
			// We ask for confirmation only for real upgrades. i18n-only upgrades are done silently.
			std::string text = format(
			   ngettext("Are you certain that you want to upgrade this %u add-on?",
			            "Are you certain that you want to upgrade these %u add-ons?", nr_full_updates),
			   nr_full_updates);
			text += '\n';
			for (const auto& pair : upgrades) {
				if (pair.second) {
					text += format(_("\n• %1$s (%2$s) by %3$s"), pair.first->descname(),
					               (pair.first->verified ? _("verified") : _("NOT VERIFIED")),
					               pair.first->author());
				}
			}
			UI::WLMessageBox w(&get_topmost_forefather(), UI::WindowStyle::kFsMenu, _("Upgrade All"),
			                   text, UI::WLMessageBox::MBoxType::kOkCancel);
			if (w.run<UI::Panel::Returncodes>() != UI::Panel::Returncodes::kOk) {
				return;
			}
		}
		for (const auto& pair : upgrades) {
			install_or_upgrade(pair.first, !pair.second);
		}
		install_websitemaps_translations_if_needed();
		rebuild_installed();
		rebuild_browse();
		update_dependency_errors();
	});

	move_up_.sigclicked.connect([this]() {
		const auto& info = selected_installed_addon();
		auto it = AddOns::g_addons.begin();
		while (it->first->internal_name != info->internal_name) {
			++it;
		}
		const bool state = it->second;
		it = AddOns::g_addons.erase(it);
		--it;
		AddOns::g_addons.insert(it, std::make_pair(info, state));
		rebuild_installed();
		update_dependency_errors();
		focus_installed_addon_row(info);
	});
	move_down_.sigclicked.connect([this]() {
		const auto& info = selected_installed_addon();
		auto it = AddOns::g_addons.begin();
		while (it->first->internal_name != info->internal_name) {
			++it;
		}
		const bool state = it->second;
		it = AddOns::g_addons.erase(it);
		++it;
		AddOns::g_addons.insert(it, std::make_pair(info, state));
		rebuild_installed();
		update_dependency_errors();
		focus_installed_addon_row(info);
	});
	move_top_.sigclicked.connect([this]() {
		const auto& info = selected_installed_addon();
		auto it = AddOns::g_addons.begin();
		while (it->first->internal_name != info->internal_name) {
			++it;
		}
		const bool state = it->second;
		it = AddOns::g_addons.erase(it);
		AddOns::g_addons.insert(AddOns::g_addons.begin(), std::make_pair(info, state));
		rebuild_installed();
		update_dependency_errors();
		focus_installed_addon_row(info);
	});
	move_bottom_.sigclicked.connect([this]() {
		const auto& info = selected_installed_addon();
		auto it = AddOns::g_addons.begin();
		while (it->first->internal_name != info->internal_name) {
			++it;
		}
		const bool state = it->second;
		it = AddOns::g_addons.erase(it);
		AddOns::g_addons.emplace_back(info, state);
		rebuild_installed();
		update_dependency_errors();
		focus_installed_addon_row(info);
	});

	launch_packager_.sigclicked.connect([this]() {
		AddOnsPackager a(fsmm_, *this);
		a.run<int>();

		// Perhaps add-ons were created or deleted
		rebuild_installed();
		update_dependency_errors();
	});

	buttons_box_.add_space(kRowButtonSpacing);
	buttons_box_.add(&upgrade_all_, UI::Box::Resizing::kExpandBoth);
	buttons_box_.add_space(kRowButtonSpacing);
	buttons_box_.add(&refresh_, UI::Box::Resizing::kExpandBoth);
	buttons_box_.add_space(kRowButtonSpacing);
#if 0  // TODO(Nordfriese): Disabled autofix_dependencies for v1.0
	buttons_box_.add(&autofix_dependencies_, UI::Box::Resizing::kExpandBoth);
	buttons_box_.add_space(kRowButtonSpacing);
#endif
	buttons_box_.add(&ok_, UI::Box::Resizing::kExpandBoth);
	buttons_box_.add_space(kRowButtonSpacing);

	main_box_.add(&tabs_placeholder_, UI::Box::Resizing::kExpandBoth);
	main_box_.add_space(kRowButtonSpacing);
	main_box_.add(&warn_requirements_, UI::Box::Resizing::kFullSize);
	main_box_.add_space(kRowButtonSpacing);
	main_box_.add(&buttons_box_, UI::Box::Resizing::kFullSize);
	main_box_.add_space(kRowButtonSpacing);

	// prevent assert failures
	installed_addons_box_.set_size(100, 100);
	browse_addons_box_.set_size(100, 100);
	maps_box_.set_size(100, 100);
	installed_addons_inner_wrapper_.set_size(100, 100);
	browse_addons_inner_wrapper_.set_size(100, 100);
	maps_inner_wrapper_.set_size(100, 100);

	installed_addons_inner_wrapper_.set_force_scrolling(true);
	browse_addons_inner_wrapper_.set_force_scrolling(true);
	maps_inner_wrapper_.set_force_scrolling(true);

	login_button_.sigclicked.connect([this]() { login_button_clicked(); });
	update_login_button(&login_button_);

	do_not_layout_on_resolution_change();
	center_to_parent();
	rebuild_installed();
	rebuild_browse();
	rebuild_maps();
	update_dependency_errors();
}

AddOnsCtrl::~AddOnsCtrl() {
	std::string text;
	for (const auto& pair : AddOns::g_addons) {
		if (!text.empty()) {
			text += ',';
		}
		text += pair.first->internal_name + ':' + (pair.second ? "true" : "false");
	}
	set_config_string("addons", text);
	write_config();
	fsmm_.set_labels();
}

void AddOnsCtrl::login_button_clicked() {
	if (username_.empty()) {
		UI::UniqueWindow::Registry r;
		AddOnsLoginBox b(get_topmost_forefather(), UI::WindowStyle::kFsMenu);
		if (b.run<UI::Panel::Returncodes>() != UI::Panel::Returncodes::kOk) {
			return;
		}
		set_login(b.get_username(), b.get_password(), true);
	} else {
		set_login("", "", false);
	}
	update_login_button(&login_button_);
}

void AddOnsCtrl::update_login_button(UI::Button* b) {
	if (username_.empty()) {
		upload_addon_accept_.set_enabled(false);
		upload_addon_accept_.set_state(false);
		if (b != nullptr) {
			b->set_title(_("Not logged in"));
			b->set_tooltip(_("Click to log in. You can then comment and vote on add-ons and upload "
			                 "your own add-ons."));
		}
		upload_addon_.set_enabled(false);
		upload_addon_.set_tooltip(_("Please log in to upload add-ons"));
		upload_screenshot_.set_enabled(upload_addon_accept_.get_state());
		upload_screenshot_.set_tooltip(_("Please log in to upload content"));
		contact_.set_enabled(false);
		contact_.set_tooltip(_("Please log in to send an enquiry"));
	} else {
		upload_addon_accept_.set_enabled(true);
		if (b != nullptr) {
			b->set_title(format(
			   net().is_admin() ? _("Logged in as %s (admin)") : _("Logged in as %s"), username_));
			b->set_tooltip(_("Click to log out"));
		}
		const bool enable = upload_addon_accept_.get_state();
		upload_addon_.set_enabled(enable);
		upload_addon_.set_tooltip(
		   enable ? "" : _("Please tick the confirmation checkbox to upload add-ons"));
		upload_screenshot_.set_enabled(enable);
		upload_screenshot_.set_tooltip(
		   enable ? "" : _("Please tick the confirmation checkbox to upload content"));
		contact_.set_enabled(true);
		contact_.set_tooltip("");
	}
}

void AddOnsCtrl::set_login(const std::string& username,
                           const std::string& password,
                           const bool show_error) {
	if (password.empty() != username.empty()) {
		return;
	}

	username_ = username;
	try {
		net().set_login(username, password);

		if (!username.empty()) {
			set_config_string("nickname", username);
			set_config_bool("registered", true);
			set_config_string("password_sha1", password);
		}
	} catch (const std::exception& e) {
		if (username.empty()) {
			log_err("set_login (''): server error (%s)", e.what());
			if (show_error) {
				UI::WLMessageBox m(
				   &get_topmost_forefather(), UI::WindowStyle::kFsMenu, _("Server Error"),
				   format(_("Unable to connect to the server.\nError message:\n%s"), e.what()),
				   UI::WLMessageBox::MBoxType::kOk);
				m.run<UI::Panel::Returncodes>();
			}
		} else {
			log_err("set_login as '%s': access denied (%s)", username.c_str(), e.what());
			if (show_error) {
				UI::WLMessageBox m(
				   &get_topmost_forefather(), UI::WindowStyle::kFsMenu, _("Wrong Password"),
				   format(_("The entered username or password is invalid:\n%s"), e.what()),
				   UI::WLMessageBox::MBoxType::kOk);
				m.run<UI::Panel::Returncodes>();
			}
			set_login("", "", show_error);
		}
	}

	server_name_.set_text(net().server_descname());
}

inline std::shared_ptr<AddOns::AddOnInfo> AddOnsCtrl::selected_installed_addon() const {
	return dynamic_cast<InstalledAddOnRow&>(*installed_addons_box_.focused_child()).info();
}
void AddOnsCtrl::focus_installed_addon_row(std::shared_ptr<AddOns::AddOnInfo> info) {
	for (UI::Panel* p = installed_addons_box_.get_first_child(); p != nullptr;
	     p = p->get_next_sibling()) {
		if (dynamic_cast<InstalledAddOnRow&>(*p).info()->internal_name == info->internal_name) {
			p->focus();
			return;
		}
	}
	NEVER_HERE();
}

void AddOnsCtrl::think() {
	UI::UniqueWindow::think();
	check_enable_move_buttons();
}

static bool category_filter_changing = false;
void AddOnsCtrl::category_filter_browse_changed(const AddOns::AddOnCategory which) {
	// protect against recursion
	if (category_filter_changing) {
		return;
	}
	category_filter_changing = true;

	// Normal click enables the selected category and disables all others,
	// Ctrl+Click or Shift+Click disables this behaviour.
	if ((SDL_GetModState() & (KMOD_CTRL | KMOD_SHIFT)) == 0) {
		for (auto& pair : filter_browse_category_) {
			pair.second->set_state(pair.first == which);
		}
	}

	filter_browse_reset_.set_enabled(true);
	rebuild_browse();
	category_filter_changing = false;
}
void AddOnsCtrl::world_filter_maps_changed(const std::string& which) {
	// protect against recursion
	if (category_filter_changing) {
		return;
	}
	category_filter_changing = true;

	// Normal click enables the selected category and disables all others,
	// Ctrl+Click or Shift+Click disables this behaviour.
	if ((SDL_GetModState() & (KMOD_CTRL | KMOD_SHIFT)) == 0) {
		for (auto& pair : filter_maps_world_) {
			pair.second->set_state(pair.first == which);
		}
	}

	filter_maps_reset_.set_enabled(true);
	rebuild_maps();
	category_filter_changing = false;
}

void AddOnsCtrl::check_enable_move_buttons() {
	const bool enable_move_buttons = tabs_.active() == 0 &&
	                                 installed_addons_box_.focused_child() != nullptr &&
	                                 !AddOns::g_addons.empty();
	for (UI::Button* b : {&move_top_, &move_up_, &move_down_, &move_bottom_}) {
		b->set_enabled(enable_move_buttons);
	}
	if (enable_move_buttons) {
		const auto& sel = selected_installed_addon();
		if (sel->internal_name == AddOns::g_addons.begin()->first->internal_name) {
			move_top_.set_enabled(false);
			move_up_.set_enabled(false);
		}
		if (sel->internal_name == AddOns::g_addons.back().first->internal_name) {
			move_down_.set_enabled(false);
			move_bottom_.set_enabled(false);
		}
	}
}

bool AddOnsCtrl::handle_key(bool down, SDL_Keysym code) {
	if (down) {
		switch (code.sym) {
		case SDLK_RETURN:
		case SDLK_ESCAPE:
			die();
			return true;
		default:
			break;
		}
	}
	return UI::Window::handle_key(down, code);
}

void AddOnsCtrl::erase_remote(std::shared_ptr<AddOns::AddOnInfo> a) {
	for (auto& pointer : remotes_) {
		if (pointer == a) {
			pointer = remotes_.back();
			remotes_.pop_back();
			return;
		}
	}
	NEVER_HERE();
}

void AddOnsCtrl::refresh_remotes(const bool showall) {
	UI::ProgressWindow progress(this, "", "");
	const std::string step_message = _("Fetching add-ons… (%.1f%%)");

	try {
		progress.step(_("Connecting to the server…"));

		std::vector<std::string> names = net().refresh_remotes(showall);

		const int64_t nr_orig_entries = names.size();
		int64_t nr_addons = nr_orig_entries;
		remotes_.resize(nr_addons);

		for (int64_t i = 0, counter = 0; i < nr_addons; ++i, ++counter) {
			progress.step(format_l(step_message, (100.0 * counter / nr_orig_entries)));

			try {
				remotes_[i] = std::make_shared<AddOns::AddOnInfo>(net().fetch_one_remote(names[i]));
			} catch (const std::exception& e) {
				log_err("Skip add-on %s because: %s", names[i].c_str(), e.what());
				names[i] = names.back();
				names.pop_back();
				remotes_.pop_back();
				--i;
				--nr_addons;
			}
		}
	} catch (const std::exception& e) {
		std::string title = _("Server Connection Error");
		/** TRANSLATORS: This will be inserted into the string "Server Connection Error <br> by %s" */
		std::string bug = _("a networking bug");
		std::string err = format(_("Unable to fetch the list of available add-ons from "
		                           "the server!<br>Error Message: %s"),
		                         richtext_escape(e.what()));
		std::shared_ptr<AddOns::AddOnInfo> i = std::make_shared<AddOns::AddOnInfo>();
		i->unlocalized_descname = title;
		i->unlocalized_description = err;
		i->unlocalized_author = bug;
		i->descname = [title]() { return title; };
		i->description = [err]() { return err; };
		i->author = [bug]() { return bug; };
		i->upload_username = bug;
		i->upload_timestamp = std::time(nullptr);
		i->icon = g_image_cache->get(AddOns::kAddOnCategories.at(AddOns::AddOnCategory::kNone).icon);
		i->sync_safe = true;  // suppress useless warning
		remotes_ = {i};
	}

	progress.step(format(step_message, 100));

	for (auto& pair : cached_browse_rows_) {
		pair.second->die();
	}
	for (auto& pair : cached_map_rows_) {
		pair.second->die();
	}
	cached_browse_rows_.clear();
	cached_map_rows_.clear();

	rebuild_browse();
	rebuild_maps();
}

bool AddOnsCtrl::matches_filter_browse(std::shared_ptr<AddOns::AddOnInfo> info) {
	if (info->internal_name.empty()) {
		// always show error messages
		return true;
	}

	if (AddOns::kAddOnCategories.at(info->category).hide) {
		return false;  // Hidden in the main view
	}

	if (!filter_browse_category_.at(info->category)->get_state()) {
		// wrong category
		return false;
	}

	if (filter_browse_verified_.get_state() && !info->verified) {
		// not verified
		return false;
	}

	if (filter_browse_quality_.get_selected() > info->quality) {
		// too low quality
		return false;
	}

	if (filter_browse_name_.get_text().empty()) {
		// no text filter given, so we accept it
		return true;
	}
	auto array = {info->descname(), info->author(), info->upload_username, info->internal_name,
	              info->description()};
	return std::any_of(array.begin(), array.end(), [this](const std::string& text) {
		return text.find(filter_browse_name_.get_text()) != std::string::npos;
	});
}

bool AddOnsCtrl::matches_filter_maps(std::shared_ptr<AddOns::AddOnInfo> info) {
	if (info->category != AddOns::AddOnCategory::kSingleMap) {
		return false;
	}

	if (info->map_width < filter_maps_min_w_.get_value()) {
		return false;
	}
	if (info->map_height < filter_maps_min_h_.get_value()) {
		return false;
	}
	if (info->map_nr_players < filter_maps_min_players_.get_value()) {
		return false;
	}
	if (info->map_width * info->map_height < filter_maps_min_size_.get_value()) {
		return false;
	}
	if (info->map_width > filter_maps_max_w_.get_value()) {
		return false;
	}
	if (info->map_height > filter_maps_max_h_.get_value()) {
		return false;
	}
	if (info->map_nr_players > filter_maps_max_players_.get_value()) {
		return false;
	}
	if (info->map_width * info->map_height > filter_maps_max_size_.get_value()) {
		return false;
	}

	if (const auto it = filter_maps_world_.find(info->map_world_name);
	    it != filter_maps_world_.end() && !it->second->get_state()) {
		// wrong world
		return false;
	}

	if (filter_maps_name_.get_text().empty()) {
		// no text filter given, so we accept it
		return true;
	}
	auto array = {
	   info->descname(),    info->author(),   info->upload_username,       info->internal_name,
	   info->description(), info->map_hint(), info->map_uploader_comment()};
	return std::any_of(array.begin(), array.end(), [this](const std::string& text) {
		return text.find(filter_maps_name_.get_text()) != std::string::npos;
	});
}

void AddOnsCtrl::rebuild_installed() {
	const uint32_t scrollpos_i =
	   installed_addons_inner_wrapper_.get_scrollbar() != nullptr ?
	      installed_addons_inner_wrapper_.get_scrollbar()->get_scrollpos() :
	      0;

	installed_addons_box_.clear();
	assert(installed_addons_box_.get_nritems() == 0);

	for (auto& pair : cached_installed_rows_) {
		pair.second->set_visible(false);
	}

	upload_addon_.clear();
	upload_screenshot_.clear();

	size_t index = 0;
	for (const auto& pair : AddOns::g_addons) {
		if (index > 0) {
			installed_addons_box_.add_space(kRowButtonSize);
		}
		++index;

		auto row_it = cached_installed_rows_.find(pair.first->internal_name);
		if (row_it == cached_installed_rows_.end()) {
			row_it = cached_installed_rows_
			            .emplace(pair.first->internal_name,
			                     new InstalledAddOnRow(
			                        &installed_addons_box_, this, pair.first, pair.second))
			            .first;
		}

		installed_addons_box_.add(row_it->second, UI::Box::Resizing::kFullSize);
		row_it->second->set_visible(true);

		upload_addon_.add(
		   pair.first->internal_name, pair.first, pair.first->icon, false, pair.first->descname());
		upload_screenshot_.add(
		   pair.first->internal_name, pair.first, pair.first->icon, false, pair.first->descname());
	}
	tabs_.tabs()[0]->set_title(format(_("Installed (%u)"), index));

	if ((installed_addons_inner_wrapper_.get_scrollbar() != nullptr) && (scrollpos_i != 0u)) {
		installed_addons_inner_wrapper_.get_scrollbar()->set_scrollpos(scrollpos_i);
	}

	check_enable_move_buttons();

	layout();
	initialization_complete();
}

void AddOnsCtrl::rebuild_browse() {
	server_name_.set_text(net().server_descname());

	const uint32_t scrollpos_b = browse_addons_inner_wrapper_.get_scrollbar() != nullptr ?
	                                browse_addons_inner_wrapper_.get_scrollbar()->get_scrollpos() :
	                                0;

	browse_addons_box_.clear();
	assert(browse_addons_box_.get_nritems() == 0);

	for (auto& pair : cached_browse_rows_) {
		pair.second->set_visible(false);
	}

	std::vector<std::shared_ptr<AddOns::AddOnInfo>> remotes_to_show;
	for (auto& a : remotes_) {
		if (matches_filter_browse(a)) {
			remotes_to_show.emplace_back(a);
		}
	}
	std::sort(remotes_to_show.begin(), remotes_to_show.end(),
	          create_sort_functor(sort_order_browse_.get_selected()));

	std::vector<std::string> has_upgrades;

	size_t index = 0;
	for (const auto& a : remotes_to_show) {
		if (index > 0) {
			browse_addons_box_.add_space(kRowButtonSize);
		}
		++index;

		auto row_it = cached_browse_rows_.find(a->internal_name);
		if (row_it == cached_browse_rows_.end()) {
			AddOns::AddOnVersion installed;
			uint32_t installed_i18n = 0;
			for (const auto& pair : AddOns::g_addons) {
				if (pair.first->internal_name == a->internal_name) {
					installed = pair.first->version;
					installed_i18n = pair.first->i18n_version;
					break;
				}
			}

			row_it = cached_browse_rows_
			            .emplace(a->internal_name, new RemoteAddOnRow(&browse_addons_box_, this, a,
			                                                          installed, installed_i18n))
			            .first;
		}

		browse_addons_box_.add(row_it->second, UI::Box::Resizing::kFullSize);
		row_it->second->set_visible(true);

		if (row_it->second->upgradeable()) {
			has_upgrades.push_back(a->descname());
		}
	}
	tabs_.tabs()[1]->set_title(index == 0 ? _("Browse") : format(_("Browse (%u)"), index));

	if ((browse_addons_inner_wrapper_.get_scrollbar() != nullptr) && (scrollpos_b != 0u)) {
		browse_addons_inner_wrapper_.get_scrollbar()->set_scrollpos(scrollpos_b);
	}

	if (current_i18n_version_ < net().websitemaps_i18n_version()) {
		has_upgrades.emplace_back(_("Translations for website maps"));
	}

	upgrade_all_.set_title(format(_("Upgrade all (%u)"), has_upgrades.size()));
	upgrade_all_.set_enabled(!has_upgrades.empty());
	if (has_upgrades.empty()) {
		upgrade_all_.set_tooltip(_("No upgrades are available for your installed add-ons"));
	} else {
		std::string text = format(ngettext("Upgrade the following %u add-on:",
		                                   "Upgrade the following %u add-ons:", has_upgrades.size()),
		                          has_upgrades.size());
		for (const std::string& name : has_upgrades) {
			text += "<br>";
			text += format(_("• %s"), name);
		}
		upgrade_all_.set_tooltip(text);
	}

	layout();
	initialization_complete();
}

void AddOnsCtrl::rebuild_maps() {
	const uint32_t scrollpos_m = maps_inner_wrapper_.get_scrollbar() != nullptr ?
	                                maps_inner_wrapper_.get_scrollbar()->get_scrollpos() :
	                                0;

	maps_box_.clear();
	assert(maps_box_.get_nritems() == 0);

	for (auto& pair : cached_map_rows_) {
		pair.second->set_visible(false);
	}

	std::vector<std::shared_ptr<AddOns::AddOnInfo>> maps_to_show;
	for (auto& a : remotes_) {
		if (matches_filter_maps(a)) {
			maps_to_show.push_back(a);
		}
	}
	std::sort(maps_to_show.begin(), maps_to_show.end(),
	          create_sort_functor(sort_order_maps_.get_selected()));

	size_t index = 0;
	for (const auto& a : maps_to_show) {
		if (index > 0) {
			maps_box_.add_space(kRowButtonSize);
		}
		++index;

		auto row_it = cached_map_rows_.find(a->internal_name);
		if (row_it == cached_map_rows_.end()) {
			std::string map_file_path = kMapsDir;
			map_file_path += FileSystem::file_separator();
			map_file_path += kDownloadedMapsDir;
			map_file_path += FileSystem::file_separator() + a->map_file_name;

			row_it = cached_map_rows_
			            .emplace(a->internal_name,
			                     new MapRow(&maps_box_, this, a, g_fs->file_exists(map_file_path)))
			            .first;
		}

		maps_box_.add(row_it->second, UI::Box::Resizing::kFullSize);
		row_it->second->set_visible(true);
	}
	tabs_.tabs()[2]->set_title(index == 0 ? _("Website Maps") :
	                                        format(_("Website Maps (%u)"), index));

	if ((maps_inner_wrapper_.get_scrollbar() != nullptr) && (scrollpos_m != 0u)) {
		maps_inner_wrapper_.get_scrollbar()->set_scrollpos(scrollpos_m);
	}

	layout();
	initialization_complete();
}

void AddOnsCtrl::update_dependency_errors() {
	std::vector<std::string> warn_requirements;
	for (auto addon = AddOns::g_addons.begin(); addon != AddOns::g_addons.end(); ++addon) {
		if (!addon->second) {
			// Disabled, so we don't care about dependencies
			continue;
		}
		for (const std::string& requirement : addon->first->requirements) {
			auto search_result = AddOns::g_addons.end();
			bool too_late = false;
			for (auto search = AddOns::g_addons.begin(); search != AddOns::g_addons.end(); ++search) {
				if (search->first->internal_name == requirement) {
					search_result = search;
					break;
				}
				if (search == addon) {
					assert(!too_late);
					too_late = true;
				}
			}
			if (search_result == AddOns::g_addons.end()) {
				warn_requirements.push_back(
				   format(_("• ‘%1$s’ requires ‘%2$s’ which could not be found"),
				          addon->first->descname(), requirement));
			} else {
				if (!search_result->second &&
				    AddOns::require_enabled(addon->first->category, search_result->first->category)) {
					warn_requirements.push_back(format(_("• ‘%1$s’ requires ‘%2$s’ which is disabled"),
					                                   addon->first->descname(),
					                                   search_result->first->descname()));
				}
				if (too_late &&
				    AddOns::order_matters(addon->first->category, search_result->first->category)) {
					warn_requirements.push_back(
					   format(_("• ‘%1$s’ requires ‘%2$s’ which is listed below the requiring add-on"),
					          addon->first->descname(), search_result->first->descname()));
				}
			}
			// Also warn if the add-on's requirements are present in the wrong order
			// (e.g. when A requires B,C but they are ordered C,B,A)
			for (const std::string& previous_requirement : addon->first->requirements) {
				if (previous_requirement == requirement) {
					break;
				}
				// check if `previous_requirement` comes before `requirement`
				const AddOns::AddOnInfo* prev = nullptr;
				const AddOns::AddOnInfo* next = nullptr;
				too_late = false;
				for (const AddOns::AddOnState& a : AddOns::g_addons) {
					if (a.first->internal_name == previous_requirement) {
						prev = a.first.get();
						break;
					}
					if (a.first->internal_name == requirement) {
						next = a.first.get();
						too_late = true;
					}
				}
				if (prev != nullptr) {
					assert(!too_late || next != nullptr);
					if (too_late && AddOns::order_matters(prev->category, next->category)) {
						warn_requirements.push_back(format(
						   _("• ‘%1$s’ requires first ‘%2$s’ and then ‘%3$s’, but they are "
						     "listed in the wrong order"),
						   addon->first->descname(), prev->descname(), search_result->first->descname()));
					}
				}
			}
		}
	}
	if (warn_requirements.empty()) {
		warn_requirements_.set_text("");
		warn_requirements_.set_tooltip("");
		try {
			Widelands::Game game;
			game.descriptions();
		} catch (const std::exception& e) {
			warn_requirements_.set_text(
			   format(_("An enabled add-on is defective. No games can be started with the "
			            "current configuration.\nError message:\n%s"),
			          e.what()));
		}
	} else {
		const unsigned nr_warnings = warn_requirements.size();
		std::string list;
		for (const std::string& msg : warn_requirements) {
			if (!list.empty()) {
				list += "<br>";
			}
			list += msg;
		}
		warn_requirements_.set_text(format(
		   "<rt><p>%s</p><p>%s</p></rt>",
		   g_style_manager->font_style(UI::FontStyle::kFsMenuInfoPanelHeading)
		      .as_font_tag(format(
		         ngettext("%u Dependency Error", "%u Dependency Errors", nr_warnings), nr_warnings)),
		   g_style_manager->font_style(UI::FontStyle::kFsMenuInfoPanelParagraph).as_font_tag(list)));
		warn_requirements_.set_tooltip(_("Add-Ons with dependency errors may work incorrectly or "
		                                 "prevent games and maps from loading."));
	}
	// TODO(Nordfriese): Disabled autofix_dependencies for v1.0
	// autofix_dependencies_.set_enabled(!warn_requirements.empty());
	layout();
}

void AddOnsCtrl::clear_cache_for_installed(const std::string& addon) {
	const auto it = cached_installed_rows_.find(addon);
	if (it != cached_installed_rows_.end()) {
		it->second->die();
		cached_installed_rows_.erase(it);
	}
}

void AddOnsCtrl::clear_cache_for_browse(const std::string& addon) {
	const auto it = cached_browse_rows_.find(addon);
	if (it != cached_browse_rows_.end()) {
		it->second->die();
		cached_browse_rows_.erase(it);
	}
}

void AddOnsCtrl::clear_cache_for_map(const std::string& addon) {
	const auto it = cached_map_rows_.find(addon);
	if (it != cached_map_rows_.end()) {
		it->second->die();
		cached_map_rows_.erase(it);
	}
}

void AddOnsCtrl::layout() {
	if (!is_minimal()) {
		main_box_.set_size(get_inner_w(), get_inner_h());

		warn_requirements_.set_visible(!warn_requirements_.get_text().empty());

		// Box layouting does not work well together with this scrolling tab panel, so we
		// use a plain Panel as a fixed-size placeholder which is layouted by the box and
		// we manually position and resize the real tab panel on top of the placeholder.
		tabs_.set_pos(Vector2i(tabs_placeholder_.get_x(), tabs_placeholder_.get_y()));
		tabs_.set_size(tabs_placeholder_.get_w(), tabs_placeholder_.get_h());

		installed_addons_outer_wrapper_.set_max_size(
		   tabs_placeholder_.get_w(), tabs_placeholder_.get_h() - 2 * kRowButtonSize);
		browse_addons_inner_wrapper_.set_max_size(
		   tabs_placeholder_.get_w(),
		   tabs_placeholder_.get_h() - 2 * kRowButtonSize - browse_addons_buttons_box_.get_h());
		maps_inner_wrapper_.set_max_size(
		   tabs_placeholder_.get_w(),
		   tabs_placeholder_.get_h() - 2 * kRowButtonSize - maps_buttons_box_.get_h());

		login_button_.set_size(get_inner_w() / 3, login_button_.get_h());
		login_button_.set_pos(Vector2i(get_inner_w() - login_button_.get_w(), 0));
		int w;
		int h;
		server_name_.get_desired_size(&w, &h);
		server_name_.set_pos(Vector2i(login_button_.get_x() - w - kRowButtonSpacing,
		                              login_button_.get_y() + (login_button_.get_h() - h) / 2));
	}

	UI::Window::layout();
}

std::shared_ptr<AddOns::AddOnInfo> AddOnsCtrl::find_remote(const std::string& name) {
	for (auto& r : remotes_) {
		if (r->internal_name == name) {
			return r;
		}
	}
	return nullptr;
}

bool AddOnsCtrl::is_remote(const std::string& name) const {
	if (remotes_.size() <= 1) {
		// No data available
		return true;
	}
	return std::any_of(remotes_.begin(), remotes_.end(),
	                   [&name](const auto& r) { return r->internal_name == name; });
}

static void install_translation(const std::string& temp_locale_path,
                                const std::string& addon_name) {
	assert(g_fs->file_exists(temp_locale_path));

	const std::string temp_filename =
	   FileSystem::fs_filename(temp_locale_path.c_str());                         // nds.po.tmp
	const std::string locale = temp_filename.substr(0, temp_filename.find('.'));  // nds

	const std::string new_locale_dir =
	   kAddOnLocaleDir + FileSystem::file_separator() + addon_name;  // addons_i18n/name.wad
	g_fs->ensure_directory_exists(new_locale_dir);

	const std::string new_locale_path = new_locale_dir + FileSystem::file_separator() + locale +
	                                    ".po";  // addons_i18n/name.wad/nds.po

	assert(!g_fs->is_directory(new_locale_path));
	if (g_fs->file_exists(new_locale_path)) {
		g_fs->fs_unlink(new_locale_path);
	}
	assert(!g_fs->file_exists(new_locale_path));

	// move translation file from temp location to the correct place
	g_fs->fs_rename(temp_locale_path, new_locale_path);

	assert(g_fs->file_exists(new_locale_path));
	assert(!g_fs->file_exists(temp_locale_path));
}

void AddOnsCtrl::upload_addon(std::shared_ptr<AddOns::AddOnInfo> addon) {
	upload_addon_.set_list_visibility(false);

	if (remotes_.size() <= 1) {
		refresh_remotes(false);
		if (remotes_.size() <= 1) {
			// Refreshing remotes failed. Switch to the tab with the error message and abort.
			tabs_.activate(1);
			return;
		}
	}

	std::shared_ptr<AddOns::AddOnInfo> remote = find_remote(addon->internal_name);
	if (remote != nullptr) {
		if (!AddOns::is_newer_version(remote->version, addon->version)) {
			UI::WLMessageBox w(
			   &get_topmost_forefather(), UI::WindowStyle::kFsMenu, _("Error"),
			   format(_("The add-on ‘%1$s’ can not be uploaded because its version (%2$s) is not "
			            "newer than the version present on the server (%3$s)."),
			          addon->internal_name, AddOns::version_to_string(addon->version, true),
			          AddOns::version_to_string(remote->version, true)),
			   UI::WLMessageBox::MBoxType::kOk);
			w.run<UI::Panel::Returncodes>();
			return;
		}
		if (remote->category != addon->category) {
			UI::WLMessageBox w(
			   &get_topmost_forefather(), UI::WindowStyle::kFsMenu, _("Error"),
			   format(_("The add-on ‘%1$s’ can not be uploaded because its category (%2$s) does not "
			            "match the category of the version present on the server (%3$s)."),
			          addon->internal_name, AddOns::kAddOnCategories.at(addon->category).descname(),
			          AddOns::kAddOnCategories.at(remote->category).descname()),
			   UI::WLMessageBox::MBoxType::kOk);
			w.run<UI::Panel::Returncodes>();
			return;
		}
	}

	{
		UI::WLMessageBox w(&get_topmost_forefather(), UI::WindowStyle::kFsMenu, _("Upload"),
		                   format(_("Do you really want to upload the add-on ‘%s’ to the server?"),
		                          addon->internal_name),
		                   UI::WLMessageBox::MBoxType::kOkCancel);
		if (w.run<UI::Panel::Returncodes>() != UI::Panel::Returncodes::kOk) {
			return;
		}
	}

	ProgressIndicatorWindow w(
	   &get_topmost_forefather(), UI::WindowStyle::kFsMenu, addon->descname());
	w.set_message_1(format(_("Uploading ‘%s’…"), addon->descname()));
	try {
		int64_t nr_files = 0;
		net().upload_addon(
		   addon->internal_name,
		   [this, &w, &nr_files](const std::string& f, const int64_t l) {
			   w.set_message_2(f);
			   w.set_message_3(format(_("%1% / %2%"), l, nr_files));
			   w.progressbar().set_state(l);
			   do_redraw_now();
			   if (w.is_dying()) {
				   throw OperationCancelledByUserException();
			   }
		   },
		   [&w, &nr_files](const std::string& /* unused */, const int64_t l) {
			   w.progressbar().set_total(l);
			   nr_files = l;
		   });
		if (remote != nullptr) {
			*remote = net().fetch_one_remote(remote->internal_name);
		}
		clear_cache_for_browse(remote->internal_name);
		rebuild_browse();
	} catch (const OperationCancelledByUserException&) {
		log_info("upload addon %s cancelled by user", addon->internal_name.c_str());
	} catch (const AddOns::IllegalFilenamesException& illegal) {
		log_warn("upload addon %s contains illegal filenames:", addon->internal_name.c_str());

		std::string message;
		for (const std::string& name : illegal.illegal_names) {
			log_warn("\t- %s", name.c_str());
			message += as_listitem(format(_("‘%s’"), name), UI::FontStyle::kFsMenuInfoPanelParagraph);
		}

		message = format(
		   "<rt>%1$s</rt>",
		   g_style_manager->font_style(UI::FontStyle::kFsMenuInfoPanelParagraph)
		      .as_font_tag(format(
		         "<p>%s</p><vspace gap=%d><p>%s</p><vspace gap=%d><p>%s</p>",
		         format(_("The add-on ‘%s’ may not be uploaded to the server because the following "
		                  "filenames contained in the add-on are not allowed:"),
		                addon->internal_name),
		         kRowButtonSize, message, kRowButtonSize,
		         _("Filenames may only contain alphanumeric characters (A-Z, a-z, 0-9) and simple "
		           "punctuation "
		           "(period, hyphen, and underscore; not multiple periods). Other characters such as "
		           "whitespace are not permitted. Filenames may not exceed 80 characters."))));

		w.set_visible(false);
		UI::WLMessageBox m(&get_topmost_forefather(), UI::WindowStyle::kFsMenu, _("Error"), message,
		                   UI::WLMessageBox::MBoxType::kOk);
		m.run<UI::Panel::Returncodes>();

	} catch (const std::exception& e) {
		log_err("upload addon %s: %s", addon->internal_name.c_str(), e.what());
		w.set_visible(false);
		UI::WLMessageBox m(
		   &get_topmost_forefather(), UI::WindowStyle::kFsMenu, _("Error"),
		   format(_("The add-on ‘%1$s’ could not be uploaded to the server.\n\nError Message:\n%2$s"),
		          addon->internal_name, e.what()),
		   UI::WLMessageBox::MBoxType::kOk);
		m.run<UI::Panel::Returncodes>();
	}
}

void AddOnsCtrl::install_map(std::shared_ptr<AddOns::AddOnInfo> remote) {
	g_fs->ensure_directory_exists(kTempFileDir);
	std::string temp_file = kTempFileDir + FileSystem::file_separator() + timestring() + ".addon." +
	                        remote->internal_name + kTempFileExtension;
	if (g_fs->file_exists(temp_file)) {
		g_fs->fs_unlink(temp_file);
	}

	bool success = false;
	try {
		net().download_map(remote->internal_name, temp_file);
		success = true;
	} catch (const OperationCancelledByUserException&) {
		log_info("install map %s cancelled by user", remote->internal_name.c_str());
	} catch (const std::exception& e) {
		log_err("install map %s: %s", remote->internal_name.c_str(), e.what());
		UI::WLMessageBox m(
		   &get_topmost_forefather(), UI::WindowStyle::kFsMenu, _("Error"),
		   format(
		      _("The map ‘%1$s’ could not be downloaded from the server.\n\nError Message:\n%2$s"),
		      remote->internal_name, e.what()),
		   UI::WLMessageBox::MBoxType::kOk);
		m.run<UI::Panel::Returncodes>();
	}

	if (!success) {
		g_fs->fs_unlink(temp_file);
		return;
	}

	std::string new_path = kMapsDir + FileSystem::file_separator() + kDownloadedMapsDir;
	g_fs->ensure_directory_exists(new_path);
	new_path += FileSystem::file_separator();
	new_path += remote->map_file_name;

	if (g_fs->file_exists(new_path)) {
		g_fs->fs_unlink(new_path);
	}
	g_fs->fs_rename(temp_file, new_path);

	install_websitemaps_translations_if_needed();

	rebuild_maps();
	update_dependency_errors();
}

// TODO(Nordfriese): install_or_upgrade() should also (recursively) install the add-on's
// requirements
void AddOnsCtrl::install_or_upgrade(std::shared_ptr<AddOns::AddOnInfo> remote,
                                    const bool only_translations) {
	ProgressIndicatorWindow w(
	   &get_topmost_forefather(), UI::WindowStyle::kFsMenu, remote->descname());
	w.set_message_1(format(_("Downloading ‘%s’…"), remote->descname()));

	g_fs->ensure_directory_exists(kAddOnDir);

	bool need_to_rebuild_texture_atlas = false;
	bool enable_theme = false;
	if (!only_translations) {
		std::string temp_dir = kTempFileDir + FileSystem::file_separator() + timestring() +
		                       ".addon." + remote->internal_name + kTempFileExtension;
		if (g_fs->file_exists(temp_dir)) {
			g_fs->fs_unlink(temp_dir);
		}

		bool success = false;
		g_fs->ensure_directory_exists(temp_dir);
		try {
			const std::string size = filesize_string(remote->total_file_size);
			w.progressbar().set_total(remote->total_file_size);
			net().download_addon(remote->internal_name, temp_dir,
			                     [this, &w, size](const std::string& f, const int64_t l) {
				                     w.set_message_2(f);
				                     w.set_message_3(format(_("%1% / %2%"), filesize_string(l), size));
				                     w.progressbar().set_state(l);
				                     do_redraw_now();
				                     if (w.is_dying()) {
					                     throw OperationCancelledByUserException();
				                     }
			                     });
			success = true;
		} catch (const OperationCancelledByUserException&) {
			log_info("install addon %s cancelled by user", remote->internal_name.c_str());
		} catch (const std::exception& e) {
			log_err("install addon %s: %s", remote->internal_name.c_str(), e.what());
			w.set_visible(false);
			UI::WLMessageBox m(
			   &get_topmost_forefather(), UI::WindowStyle::kFsMenu, _("Error"),
			   format(
			      _("The add-on ‘%1$s’ could not be downloaded from the server. Installing/upgrading "
			        "this add-on will be skipped.\n\nError Message:\n%2$s"),
			      remote->internal_name, e.what()),
			   UI::WLMessageBox::MBoxType::kOk);
			m.run<UI::Panel::Returncodes>();
		}
		if (!success) {
			g_fs->fs_unlink(temp_dir);
			return;
		}

		const std::string new_path = kAddOnDir + FileSystem::file_separator() + remote->internal_name;
		if (g_fs->file_exists(new_path)) {
			g_fs->fs_unlink(new_path);
		}
		g_fs->fs_rename(temp_dir, new_path);

		need_to_rebuild_texture_atlas = remote->requires_texture_atlas_rebuild();
		bool found = false;
		for (auto& pair : AddOns::g_addons) {
			if (pair.first->internal_name == remote->internal_name) {
				pair.first = AddOns::preload_addon(remote->internal_name);
				enable_theme =
				   (remote->category == AddOns::AddOnCategory::kTheme &&
				    template_dir() == AddOns::theme_addon_template_dir(remote->internal_name));
				found = true;
				break;
			}
		}
		if (!found) {
			enable_theme = (remote->category == AddOns::AddOnCategory::kTheme);
			AddOns::g_addons.emplace_back(AddOns::preload_addon(remote->internal_name), true);
		}
	}

	install_translations(remote->internal_name, remote->i18n_version, w);

	WLApplication::get().init_plugin_shortcuts();
	if (need_to_rebuild_texture_atlas) {
		g_gr->rebuild_texture_atlas();
	}
	if (enable_theme) {
		AddOns::update_ui_theme(AddOns::UpdateThemeAction::kEnableArgument, remote->internal_name);
		get_topmost_forefather().template_directory_changed();
	}
	if (remote->category == AddOns::AddOnCategory::kUIPlugin) {
		fsmm_.reinit_plugins();
	}

	clear_cache_for_installed(remote->internal_name);
	clear_cache_for_browse(remote->internal_name);
	update_dependency_errors();
	rebuild_installed();
	rebuild_browse();
}

bool AddOnsCtrl::install_translations(const std::string& name,
                                      uint32_t new_i18n_version,
                                      ProgressIndicatorWindow& progress) {
	std::string temp_dir = kTempFileDir + FileSystem::file_separator() + timestring() +
	                       ".addoni18n." + name + kTempFileExtension;
	assert(!g_fs->file_exists(temp_dir));
	g_fs->ensure_directory_exists(temp_dir);
	bool success = false;
	try {
		progress.progressbar().set_state(0);
		progress.progressbar().set_total(1);
		int64_t nr_translations = 0;
		progress.set_message_3("");
		net().download_i18n(
		   name, temp_dir,
		   [this, &progress, &nr_translations](const std::string& f, const int64_t l) {
			   progress.set_message_2(f);
			   progress.set_message_3(format(_("%1% / %2%"), l, nr_translations));
			   progress.progressbar().set_state(l);
			   do_redraw_now();
			   if (progress.is_dying()) {
				   throw OperationCancelledByUserException();
			   }
		   },
		   [&progress, &nr_translations](const std::string& /* unused */, const int64_t l) {
			   nr_translations = l;
			   progress.progressbar().set_total(std::max<int64_t>(l, 1));
		   });

		for (const std::string& n : g_fs->list_directory(temp_dir)) {
			install_translation(n, name);
		}
		i18n::clear_addon_translations_cache(name);
		for (auto& pair : AddOns::g_addons) {
			if (pair.first->internal_name == name) {
				pair.first->i18n_version = new_i18n_version;
				break;
			}
		}
		Profile prof(kAddOnLocaleVersions.c_str());
		prof.pull_section("global").set_natural(name.c_str(), new_i18n_version);
		prof.write(kAddOnLocaleVersions.c_str(), false);

		success = true;
	} catch (const OperationCancelledByUserException&) {
		log_info("install translations for %s cancelled by user", name.c_str());
		success = false;
	} catch (const std::exception& e) {
		log_err("install translations for %s: %s", name.c_str(), e.what());
		success = false;
		progress.set_visible(false);
		UI::WLMessageBox m(
		   &get_topmost_forefather(), UI::WindowStyle::kFsMenu, _("Error"),
		   format(_("The translations for the add-on ‘%1$s’ could not be downloaded from the "
		            "server. Installing/upgrading "
		            "the translations will be skipped.\n\nError Message:\n%2$s"),
		          name, e.what()),
		   UI::WLMessageBox::MBoxType::kOk);
		m.run<UI::Panel::Returncodes>();
	}
	g_fs->fs_unlink(temp_dir);
	return success;
}

void AddOnsCtrl::install_websitemaps_translations_if_needed() {
	if (g_fs->list_directory(kDownloadedMapsDirFull).empty()) {
		return;  // No website maps installed
	}

	const uint32_t remote_i18n_version = net().websitemaps_i18n_version();

	if (current_i18n_version_ >= remote_i18n_version) {
		return;  // No newer version available
	}

	ProgressIndicatorWindow progress(
	   &get_topmost_forefather(), UI::WindowStyle::kFsMenu, _("Website Maps Translations"));
	progress.set_message_1(_("Downloading website maps translations…"));
	if (install_translations("websitemaps", remote_i18n_version, progress)) {
		current_i18n_version_ = remote_i18n_version;
	}
}

#if 0  // TODO(Nordfriese): Disabled autofix_dependencies for v1.0
// UNTESTED
// Automatically fix all dependency errors by reordering add-ons and downloading missing ones.
// We make no guarantees inhowfar the existing order is preserved
// (e.g. if A currently comes before B, it may come after B after reordering even if
// there is no direct or indirect dependency relation between A and B).
void AddOnsCtrl::autofix_dependencies() {
	std::set<std::string> missing_requirements;

// Step 1: Enable all dependencies
step1:
	for (const AddOns::AddOnState& addon_to_fix : AddOns::g_addons) {
		if (addon_to_fix.second) {
			bool anything_changed = false;
			bool found = false;
			for (const std::string& requirement : addon_to_fix.first->requirements) {
				for (AddOns::AddOnState& a : AddOns::g_addons) {
					if (a.first->internal_name == requirement) {
						found = true;
						if (!a.second) {
							a.second = true;
							anything_changed = true;
						}
						break;
					}
				}
				if (!found) {
					missing_requirements.insert(requirement);
				}
			}
			if (anything_changed) {
				// concurrent modification – we need to start over
				goto step1;
			}
		}
	}

	// Step 2: Download missing add-ons
	for (const std::string& addon_to_install : missing_requirements) {
		bool found = false;
		for (const auto& info : remotes_) {
			if (info->internal_name == addon_to_install) {
				install_or_upgrade(info, false);
				found = true;
				break;
			}
		}
		if (!found) {
			UI::WLMessageBox w(
			   &get_topmost_forefather(), UI::WindowStyle::kFsMenu, _("Error"),
			   format(_("The required add-on ‘%s’ could not be found on the server.") ,
			    addon_to_install)
			      ,
			   UI::WLMessageBox::MBoxType::kOk);
			w.run<UI::Panel::Returncodes>();
		}
	}

	// Step 3: Get all add-ons into the correct order
	std::map<std::string, AddOns::AddOnState> all_addons;

	for (const AddOns::AddOnState& aos : AddOns::g_addons) {
		all_addons[aos.first->internal_name] = aos;
	}

	std::multimap<unsigned /* number of dependencies */, AddOns::AddOnState> addons_tree;
	for (const auto& pair : all_addons) {
		addons_tree.emplace(
		   std::make_pair(count_all_dependencies(pair.first, all_addons), pair.second));
	}
	// The addons_tree now contains a list of all add-ons sorted by number
	// of (direct plus indirect) dependencies
	AddOns::g_addons.clear();
	for (const auto& pair : addons_tree) {
		AddOns::g_addons.push_back(AddOns::AddOnState(pair.second));
	}

	rebuild_installed();
	rebuild_browse();
	update_dependency_errors();
}
#endif

}  // namespace AddOnsUI
