#pragma once
// Release automation opts in explicitly. Local/fork builds identify themselves
// separately; historical copyright notices remain in LICENSE/COPYRIGHT.md.
#ifndef CUE_OFFICIAL_BUILD
#define CUE_OFFICIAL_BUILD 0
#endif
#ifndef CUE_DISTRIBUTOR
#if CUE_OFFICIAL_BUILD
#define CUE_DISTRIBUTOR ".tinypeep"
#else
#define CUE_DISTRIBUTOR "Community build"
#endif
#endif
#if CUE_OFFICIAL_BUILD
#define CUE_PRODUCT_NAME "Cue"
#else
#define CUE_PRODUCT_NAME "Cue (unofficial)"
#endif
// The Microsoft Store edition (make OFFICIAL=1 STORE=1, packaged by tools/make_msix.py).
// The Store installs and updates it, so it never asks GitHub for releases, never runs
// Cue-Setup.exe, and leaves "Open with" and file types to the package manifest. Everything
// else (interface, playback, phone remote) is the same program as the GitHub edition.
#ifndef CUE_STORE_BUILD
#define CUE_STORE_BUILD 0
#endif
// GitHub releases update official builds of the GitHub edition only.
#define CUE_GITHUB_UPDATES (CUE_OFFICIAL_BUILD && !CUE_STORE_BUILD)
// Read by tools/make_msix.py, which refuses to package a Cue.exe without it.
#if CUE_STORE_BUILD
#define CUE_EDITION_TAG "cue-edition:microsoft-store"
#else
#define CUE_EDITION_TAG "cue-edition:github"
#endif
