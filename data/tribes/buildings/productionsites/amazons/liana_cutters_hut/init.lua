push_textdomain("tribes")

local dirname = path.dirname (__file__)

wl.Descriptions():new_productionsite_type {
   name = "amazons_liana_cutters_hut",
   -- TRANSLATORS: This is a building name used in lists of buildings
   descname = pgettext ("amazons_building", "Liana Cutter’s Hut"),
   icon = dirname .. "menu.png",
   size = "small",

   buildcost = {
      log = 5,
   },
   return_on_dismantle = {
      log = 3
   },

   animation_directory = dirname,
   animations = {
      idle = {hotspot = {39, 45}},
      unoccupied = {hotspot = {39, 45}}
   },

   aihints = {
      very_weak_ai_limit = 1,
      weak_ai_limit = 2,
      basic_amount = 1,
   },

   working_positions = {
      amazons_liana_cutter = 1
   },


   programs = {
      main = {
         -- TRANSLATORS: Completed/Skipped/Did not start making clay because ...
         descname = _("cutting lianas"),
         actions = {
            -- time of worker: 12.2-37.4 sec
            -- min. time total: 12.2 + 30 = 42.2 sec
            -- max. time total: 37.4 + 30 = 67.4 sec
            "return=skipped unless economy needs liana",
            "callworker=cut",
            "sleep=duration:30s",
         },
      },
   },
   out_of_resource_notification = {
      -- TRANSLATORS: Short for "No Tree to Cut Lianas" for a resource
      title = _("No Trees"),
      heading = _("No Tree to Cut Lianas"),
      message = pgettext ("amazons_building", "The liana cutter working at this site can’t find any tree in her work area. You should consider dismantling or destroying the building or building a jungle preserver’s hut."),
      productivity_threshold = 33
   },
}

pop_textdomain()
