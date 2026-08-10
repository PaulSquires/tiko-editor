'' ========================================================================================
'' modAppHost -- implementation. See modAppHost.bi for what this is and why.
'' ========================================================================================

dim shared gAppHost   as AppHostServices
dim shared gAppNotify as AppHostNotify

'' The completeness check is written as a LIST rather than one long `andalso` chain, so that
'' a missing field can be NAMED. "The host is incomplete" sends a reader through eleven
'' fields by hand; "AppHost.CreateView is not set" does not.
''
'' fbc has no way to iterate a type's members, so the two functions below walk the same
'' fields in the same order and the pairing is maintained by hand. That is a real cost and it
'' is the reason the list is short: every field added here is a field added twice.
function AppHost_FirstMissing() as string
    if gAppHost.CreateView     = 0 then return "CreateView"
    if gAppHost.DestroyView    = 0 then return "DestroyView"
    if gAppHost.IsViewAlive    = 0 then return "IsViewAlive"
    if gAppHost.ViewSciPointer = 0 then return "ViewSciPointer"
    if gAppHost.InvalidateView = 0 then return "InvalidateView"
    if gAppHost.SetViewRedraw  = 0 then return "SetViewRedraw"
    if gAppHost.ViewScale      = 0 then return "ViewScale"
    if gAppHost.ViewHeight     = 0 then return "ViewHeight"
    if gAppHost.ViewUiFontName = 0 then return "ViewUiFontName"
    if gAppHost.ViewUiFontSize = 0 then return "ViewUiFontSize"
    if gAppHost.StyleView      = 0 then return "StyleView"
    if gAppHost.ThemeSelectionBack = 0 then return "ThemeSelectionBack"
    if gAppHost.LastErrorText  = 0 then return "LastErrorText"
    if gAppHost.LoadFileText   = 0 then return "LoadFileText"
    if gAppHost.ResolveIncludePath = 0 then return "ResolveIncludePath"
    if gAppHost.ConfirmLossySave   = 0 then return "ConfirmLossySave"
    if gAppHost.ReportWriteFailure = 0 then return "ReportWriteFailure"
    if gAppHost.AskOpenPath    = 0 then return "AskOpenPath"
    if gAppHost.AskSavePath    = 0 then return "AskSavePath"
    if gAppHost.IsFindVisible  = 0 then return "IsFindVisible"
    return ""
end function

function AppHost_IsComplete() as boolean
    return (len(AppHost_FirstMissing()) = 0)
end function


function AppNotify_FirstMissing() as string
    if gAppNotify.CloseTab          = 0 then return "CloseTab"
    if gAppNotify.OnDocumentClosing = 0 then return "OnDocumentClosing"
    if gAppNotify.OnDocumentSaved   = 0 then return "OnDocumentSaved"
    if gAppNotify.RequestBufferScan = 0 then return "RequestBufferScan"
    if gAppNotify.DocRootName       = 0 then return "DocRootName"
    if gAppNotify.UpdateMruFile     = 0 then return "UpdateMruFile"
    if gAppNotify.UpdateMruProject  = 0 then return "UpdateMruProject"
    if gAppNotify.ReloadExplorer    = 0 then return "ReloadExplorer"
    if gAppNotify.RelayoutMain      = 0 then return "RelayoutMain"
    if gAppNotify.RelayoutTopTabs   = 0 then return "RelayoutTopTabs"
    if gAppNotify.RefreshTodoList   = 0 then return "RefreshTodoList"
    return ""
end function

function AppNotify_IsComplete() as boolean
    return (len(AppNotify_FirstMissing()) = 0)
end function
