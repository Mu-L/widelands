push_textdomain("tribes")

local dirname = path.dirname (__file__)

wl.Descriptions():new_worker_type {
   name = "amazons_wilderness_keeper",
   -- TRANSLATORS: This is a worker name used in lists of workers
   descname = pgettext ("amazons_worker", "Wilderness Keeper"),
   icon = dirname .. "menu.png",
   vision_range = 2,

   buildcost = {
      amazons_carrier = 1
   },

   programs = {
      release_game = {
         -- steps from building to tree: 2-13
         -- min. worker time: 2 *  2 * 1.8 + 4.181 = 11.381 sec
         -- max. worker time: 2 * 13 * 1.8 + 4.181 = 50.981 sec
         "findobject=attrib:tree radius:10",
         "walk=object",
         "animate=release duration:4s181ms",
         "createbob=bunny badger fox lynx marten wildboar",
         "return"
      },
      breed_fish = {
         -- steps from building to water: 2-10
         -- min. worker time: 2 *  2 * 1.8 + 12.545 = 19.745 sec
         -- max. worker time: 2 * 10 * 1.8 + 12.545 = 48.545 sec
         "findspace=size:swim radius:7",
         "walk=coords",
         "animate=release duration:12s545ms",
         "breed=resource_fish radius:1",
         "return"
      }
   },

   animation_directory = dirname,
   ware_hotspot = {0, 29},
   spritesheets = {
      walk = {
         directional = true,
         hotspot = {17, 28},
         fps = 15,
         frames = 30,
         columns = 6,
         rows = 5
      },
      idle = {
         basename = "walk_se",
         hotspot = {17, 28},
         fps = 15,
         frames = 30,
         columns = 6,
         rows = 5
      },
      release = {
         hotspot = {14, 25},
         -- I apologize for the unusual frame durations in this animation…
         fps = 11,
         frames = 23,
         columns = 6,
         rows = 4
      },
   },
}

pop_textdomain()
