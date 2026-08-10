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
    if gAppHost.InvalidateView = 0 then return "InvalidateView"
    if gAppHost.SetViewRedraw  = 0 then return "SetViewRedraw"
    if gAppHost.ViewScale      = 0 then return "ViewScale"
    if gAppHost.ViewHeight     = 0 then return "ViewHeight"
    if gAppHost.ViewUiFontName = 0 then return "ViewUiFontName"
    if gAppHost.ViewUiFontSize = 0 then return "ViewUiFontSize"
    if gAppHost.AskOpenPath    = 0 then return "AskOpenPath"
    if gAppHost.AskSavePath    = 0 then return "AskSavePath"
    if gAppHost.IsFindVisible  = 0 then return "IsFindVisible"
    return ""
end function

function AppHost_IsComplete() as boolean
    return (len(AppHost_FirstMissing()) = 0)
end function


function AppNotify_FirstMissing() as string
    if gAppNotify.CloseTab = 0 then return "CloseTab"
    return ""
end function

function AppNotify_IsComplete() as boolean
    return (len(AppNotify_FirstMissing()) = 0)
end function
