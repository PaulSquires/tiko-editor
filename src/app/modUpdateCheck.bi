' #####################################################################################
' Declarations for modUpdateCheck.inc -- the version check and its reply parser.
' Split out of modRoutines.bi, which forwards to this so no call site changed.
' #####################################################################################

#pragma once

' Pure parse of the update server's reply -- see its definition for why it is strict.
declare function ParseLatestVersion( byref sBody as string ) as DWSTRING
' Handle of the in-flight update-check thread, joined when it reports back. fbc 1.10 has
' no ThreadDetach, so it has to be waited on somewhere or the thread struct leaks.
dim shared ghUpdateCheckThread as any ptr
