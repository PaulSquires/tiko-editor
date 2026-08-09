'' ========================================================================================
'' modAppConstants -- the handful of constants the APP LAYER needs and the shell also uses.
''
'' ---- WHY THIS FILE EXISTS, WHICH IS NOT THE REASON IT LOOKS LIKE -----------------------
''
'' Not tidiness, and not a general home for constants. It exists because ONE function needed
'' them from below the shell: `constructor clsConfig`.
''
'' The instance `gConfig` lives with its type, in app/clsConfig.bi, so that app-layer code
'' can name the OBJECT and not merely the TYPE. That means including the header INSTANTIATES
'' it, which means the constructor has to be linkable by anything that includes the header --
'' and the constructor sets four defaults from shell constants plus APPEXTENSION.
''
'' Those four had already been moved once, out of the header's field initialisers and into
'' the constructor, precisely so the HEADER would compile without the shell. That worked, and
'' it moved the dependency from the header into the body, where the app-standalone gate could
'' not see it: the gate compiled each file with `fbc -c` and never linked. The app layer
'' therefore reported 7 clean / 0 errors for months while being unlinkable on its own.
''
'' Found by phase 7c's shell binary, which is the first thing that ever linked the layer.
''
'' ---- WHAT IS AND IS NOT ALLOWED IN HERE ------------------------------------------------
''
'' A constant belongs here when the APP LAYER needs it. `OUTPUT_TABS_HEIGHT` is here because
'' clsConfig's default output-panel height is five rows of it -- not because it is a nice
'' place for a layout constant. The shell keeps owning every other layout constant, in
'' modDeclares.bi and the frm* headers, and there are dozens.
''
'' THE UNITS ARE UNSCALED DESIGN UNITS, like every layout constant in tiko. Anything that
'' puts one of these on screen scales it -- see the DPI rule in the guidelines.
'' ========================================================================================

#pragma once

'' The Output panel's tab strip height. clsConfig's constructor defaults
'' ShowOutputPanelHeight to five of these. modDeclares.bi derives
'' OUTPUT_PANEL_MIN_HEIGHT from it, and frmOutput positions against it.
const OUTPUT_TABS_HEIGHT = 40

'' The debugger's three splitter defaults, in BASIS POINTS of the available extent
'' (6000 = 60%). clsConfig stores the user's positions; these are where they start.
const as long FRMDEBUG_DEFPCTMAIN  = 6000    '' vertical bar, 60% across
const as long FRMDEBUG_DEFPCTLEFT  = 3200    '' globals over locals
const as long FRMDEBUG_DEFPCTRIGHT = 8300    '' call stack over watch

'' The project-file extension. A #define rather than a const because it is used to build
'' wstring literals by concatenation, and it was a #define in tiko.bas for the same reason.
#define APPEXTENSION        wstr(".tiko")
