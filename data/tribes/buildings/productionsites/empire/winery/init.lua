push_textdomain("tribes")

local dirname = path.dirname(__file__)

wl.Descriptions():new_productionsite_type {
   name = "empire_winery",
   -- TRANSLATORS: This is a building name used in lists of buildings
   descname = pgettext("empire_building", "Winery"),
   icon = dirname .. "menu.png",
   size = "medium",

   buildcost = {
      planks = 1,
      granite = 1,
      marble = 2,
      marble_column = 1
   },
   return_on_dismantle = {
      granite = 1,
      marble = 1,
      marble_column = 1
   },

   animation_directory = dirname,
   spritesheets = {
      idle = {
         frames = 1,
         columns = 1,
         rows = 1,
         hotspot = { 42, 66 },
      },
      working = {
         basename = "idle", -- TODO(GunChleoc): No animation yet.
         frames = 1,
         columns = 1,
         rows = 1,
         hotspot = { 42, 66 },
      },
   },

   aihints = {
      prohibited_till = 560,
      very_weak_ai_limit = 1,
      weak_ai_limit = 2,
      basic_amount = 1
   },

   working_positions = {
      empire_vintner = 1
   },

   inputs = {
      { name = "grape", amount = 8 }
   },

   programs = {
      main = {
         -- TRANSLATORS: Completed/Skipped/Did not start making wine because ...
         descname = _("making wine"),
         actions = {
            -- time total: 30.4 + 30 + 3.6 = 64 sec
            -- Grapes are only needed for wine, so no need to check if wine is needed
            "consume=grape:2",
            "sleep=duration:30s400ms",
            "playsound=sound/empire/winebubble priority:40% allow_multiple",
            "animate=working duration:30s",
            "produce=wine"
         }
      },
   },
}

pop_textdomain()
