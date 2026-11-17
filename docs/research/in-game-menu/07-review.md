# The review of the native menus (2026-11-17)

A workflow: four reviewers (the guest hooks; the plaque bank and override; the menu; the title row), two skeptics per
finding. 11 findings, 6 survived, all fixed in hle.c / frontend.cpp / plaques.*:

| Finding | Fix |
|---|---|
| (medium) The fallback labels (no Options picture) were made again in the frame after the page's exit (B or Start), outliving the page and showing over play | tome_stops_frame makes labels only while the page's exit word r13-24308 is still 0x801016D8 |
| (medium) A plaque bank restored from another build's save state kept its old stubs: blank plaques, no words | the bank's canary mixes in the stubs' version, size and count (PLAQUE_CANARY_WORD), so another build's bank is rewritten |
| (medium) The title's ninth row was added in every language, painted only on the English Options picture: an invisible row elsewhere | title_on_options requires the language word r13-26284 == 0 (English) |
| (low) Player 2's choose / denied sounds in their copy Tome started in the copy's thrown-away sound state | tome_stops_sound starts them in the real memory during a copy turn, as tome_copy_sound_live does |
| (low) ED_PAGE_TEST_MS left its deadline armed when a page closed sooner | cleared after run_menu |
| (low) "More Options" went unlit at the start of the fade out to the main menu | title_more_draw uses the draw's own cursor argument (r3) |

Refuted: stale copy-Tome label pointers; a missing hand font; no way to reach save states / Quit with the debug menu off
when the Tome can't be opened; co-op toggled from a page not running rooms_follow_coop; the two atomics of
frontend_open_page.
