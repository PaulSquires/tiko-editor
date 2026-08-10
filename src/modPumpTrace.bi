'' ========================================================================================
'' modPumpTrace -- THE PUMP ORACLE FOR PHASE 7c STEP 2.
''
'' Records which filter CLAIMED each message, in every message loop in the application, and
'' writes a summary when the process exits. Env-gated on TIKO_PUMP_TRACE=1; does nothing
'' otherwise.
''
'' ---- WHY THIS EXISTS -------------------------------------------------------------------
''
'' Step 2 ports the pump. frmMain's loop is twelve ordered filter claims, three accelerator
'' tables and an IsDialogMessage, and every one of them carries a comment explaining what it
'' claims and why it sits where it does. Those comments are the best documentation in the
'' codebase and they are still only a claim ABOUT the code.
''
'' The census this feeds asks a question the comments cannot answer: WHICH OF THESE FILTERS
'' ACTUALLY FIRES? A filter that never claims anything cannot be verified as ported -- the
'' port would be equally green whether it reproduced the behaviour or dropped it. Step 1
'' shipped three assertions that passed with their own fix reverted, all three from reasoning
'' about code instead of running it, so the ported pump gets numbers from the running editor
'' the same way the layout did.
''
'' ---- WHAT IT COUNTS, AND WHY IT IS NOT A LOG -------------------------------------------
''
'' A line per message would be a line per WM_MOUSEMOVE, and five minutes of ordinary editing
'' would bury the interesting events under hundreds of thousands of them. So this counts:
'' one slot per (pump, claimant) pair, a total, and up to PUMPTRACE_MAXSAMPLES DISTINCT
'' message descriptors per slot so the census can say WHAT each filter claimed, not merely
'' how often.
''
'' THE PUMP IS PART OF THE KEY, not just the claimant. PsTextBox_FilterMessage firing in
'' frmMain's loop and the same function firing in the Options dialog's loop are two different
'' facts: the first says the main pump needs the filter, the second says a dialog does. The
'' port answers them separately -- frmMain becomes a PsSurface pump and the dialogs become
'' PsModalHost -- so conflating them would erase the distinction the census is for.
''
'' That is also why the hooks are at the CALL SITES rather than inside the filter functions.
'' Instrumenting the seven function bodies would be a much smaller diff and would lose the
'' caller entirely; it would also put trace code inside files that are kept byte-identical to
'' their standalone PsControls repos.
''
'' ---- COST WHEN OFF ---------------------------------------------------------------------
''
'' NOT ZERO, and saying so plainly: one function call and two boolean tests per filter per
'' message. That is roughly forty calls per message in frmMain's loop, against filters that
'' each do real work, so it is not measurable -- but it is not nothing, and a claim of "no
'' cost when off" would be the kind of thing this file exists to distrust.
''
'' The trace is written by an atexit handler, so a normal quit and an `end` both produce it.
'' ========================================================================================

#pragma once

const PUMPTRACE_MAXSLOTS   = 96
const PUMPTRACE_MAXSAMPLES = 12

'' Set by PumpTrace_Init from the environment. Read on every call, so it is checked rather
'' than assumed -- see the cost note above.
extern gPumpTraceOn as boolean

'' Reads TIKO_PUMP_TRACE and arms the atexit writer. Safe to call more than once.
declare sub PumpTrace_Init()

'' THE PASS-THROUGH. Wraps a filter call so the call site keeps its shape:
''
''     if PumpTrace( PsTextBox_FilterMessage(@uMsg), "frmMain", "PsTextBox", @uMsg ) then continue do
''
'' Returns bClaimed unchanged, always. Records only when bClaimed is true AND tracing is on.
declare function PumpTrace( byval bClaimed as boolean, _
                            byval zPump as zstring ptr, _
                            byval zWho as zstring ptr, _
                            byval pMsg as MSG ptr ) as boolean

'' For the points that are not a boolean filter -- the accelerator tables, IsDialogMessage,
'' and the final DispatchMessage. Same recording, no pass-through.
declare sub PumpTrace_Note( byval zPump as zstring ptr, _
                            byval zWho as zstring ptr, _
                            byval pMsg as MSG ptr )

'' Counts a message ENTERING a pump, so the summary can report what share each filter took.
declare sub PumpTrace_Enter( byval zPump as zstring ptr )

'' Writes the summary to TIKO_PUMP_TRACE_OUT, or to pump-trace.txt beside the exe. Called
'' automatically at exit; exposed for tests.
declare sub PumpTrace_Write()
