'' ========================================================================================
'' clsConfig -- the CONSTRUCTOR, and only the constructor.
''
'' ---- WHY THE CONSTRUCTOR IS DOWN HERE AND THE OTHER ~90 METHODS ARE NOT ----------------
''
'' app/clsConfig.bi:281 is `dim shared gConfig as clsConfig`. The instance lives with its
'' type deliberately, so that app-layer code can name the OBJECT and not merely the TYPE --
'' modMenuDefinitions.inc reads gConfig.MRU, .Tools, .MRUPROJECT and .ExplorerPositionRight.
''
'' The consequence is the reason this file exists: INCLUDING THE HEADER INSTANTIATES IT, so
'' anything that includes app/clsConfig.bi must be able to LINK a constructor. While the
'' constructor sat in src/clsConfig.inc, that meant every consumer of the app layer needed
'' the shell -- AfxGuid, HWND_FRMMAIN, and 40-odd frm* headers -- to link.
''
'' The rest of clsConfig stays in src/clsConfig.inc. It reads and writes real files, rebuilds
'' accelerator tables and talks to frmMain; none of that is app-layer work and none of it is
'' reached by merely declaring the object.
''
'' ---- HOW THIS WAS FOUND, WHICH MATTERS MORE THAN THE FIX -------------------------------
''
'' The four UI defaults below had ALREADY been moved once, out of the header's field
'' initialisers and into the constructor, precisely so the header would compile without the
'' shell. That worked. It also moved the dependency from the header into the BODY, and
'' _check_app_standalone.bat compiles each file with `fbc -c` and never links -- so the gate
'' could not see the difference and reported 7 clean / 0 errors throughout.
''
'' Phase 7c's shell binary is the first thing that ever linked the layer, and it failed on
'' one symbol: `CLSCONFIG::CLSCONFIG()`. Nine of the sixteen app headers linked; the seventh
'' and everything after it failed on that one name.
''
'' The gate now links as well as compiles. A compile-only gate cannot answer a linkage
'' question, and this layer's whole purpose is a linkage claim.
'' ========================================================================================

#include once "app/clsConfig.bi"
#include once "app/modAppConstants.bi"
#include once "app/modPaths.bi"

constructor clsConfig
    dim as DWSTRING exe_path = PsExePath
    ConfigFilename            = exe_path & "settings\settings.ini"
    FBKeywordsFilename        = exe_path & "settings\keywords\freebasic_keywords.txt"
    WinApiKeywordsFilename    = exe_path & "settings\keywords\winapi_keywords.txt"
    ExtraKeywordsFilename     = exe_path & "settings\keywords\extra_keywords.txt"
    FBKeywordsDefaultFilename = exe_path & "settings\keywords\freebasic_keywords_default.txt"
    FBCodetipsFilename        = exe_path & "settings\keywords\codetips.ini"
    UntitledProjectFilename   = exe_path & "settings\default" & APPEXTENSION

    ' ---- UI LAYOUT DEFAULTS -----------------------------------------------------
    ' The split is between the FIELD and its DEFAULT. A stored panel height and a
    ' stored splitter percentage are portable user preferences; how tall the output
    ' tabs are, and where a debugger splitter should start, are statements about the
    ' UI. That distinction still holds and nothing about it has changed.
    '
    ' What changed is WHERE the four values are declared. They were shell constants
    ' in modDeclares.bi and frmDebug.bi, which made this constructor unlinkable
    ' without the shell -- so they moved DOWN to app/modAppConstants.bi and the two
    ' shell headers now include that. The shell still uses them everywhere it did;
    ' it just no longer owns them.
    ShowOutputPanelHeight     = OUTPUT_TABS_HEIGHT * 5
    DebugSplitMain            = FRMDEBUG_DEFPCTMAIN
    DebugSplitLeft            = FRMDEBUG_DEFPCTLEFT
    DebugSplitRight           = FRMDEBUG_DEFPCTRIGHT
end constructor
