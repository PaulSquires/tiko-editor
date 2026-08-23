'' Parser test INPUT (never compiled by build.bat) - proc body location.
'' A proc WITH a body must report the implementation's file/line (the old
'' declare-location behavior sent hosts to the .bi for every member proc):
''
'' Expected:
''   GetThing  FUNCTION  file = THIS file (not the .bi), lineNum = first
''             body line, body=<range in this file>, parent = ProcBodyT
''   fld       FIELD     file = the .bi (declare-only symbols keep their
''             DECLARE location)
#include once "_testfile_procbody.bi"

dim shared gProcBody as ProcBodyT

function ProcBodyT.GetThing( byval n as long ) as string
    dim as string sLocal
    function = sLocal
end function
