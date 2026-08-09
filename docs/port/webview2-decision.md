# WebView2 — settled, and it was never the blocker

`d2-decision.md` named WebView2 **"the one constraint that cannot be engineered away"** and said
it should drive D2's answer rather than be discovered halfway. This page is that investigation.

**Its conclusion: WebView2 does not constrain D2 at all.** Neither Shape A nor Shape B is
blocked by it, and three of the four sentences the other page used to describe it are wrong.
The real question turned out to be a different one, with a cheaper answer.

---

## WHAT d2-decision.md SAID, AND WHAT IS ACTUALLY THERE

> **The hard blocker is WebView2.** `frmHelpCenter.inc` parents an AfxNova `CWebView2` to a
> `CreateWindowExW(0, "STATIC", …)` host. It needs a real HWND permanently and there is no SDL3
> path. Under A the Help Center becomes an embedded native child window or it is dropped.

| the claim | what the code says |
| --- | --- |
| "there is no SDL3 path" | **There is.** `SDL_PROP_WINDOW_WIN32_HWND_POINTER` is in the binding today (`src/bind/SDL3/SDL3/SDL_video.bi`). SDL3 hands over the native HWND on request. |
| "becomes an embedded native child window" | **It never has to be embedded.** `frmHelpCenter` is created `WS_POPUP or WS_OVERLAPPEDWINDOW` — a SEPARATE TOP-LEVEL WINDOW, opened on demand from the Help menu and F1. It is not a pane inside `frmMain` and does not become one. |
| "needs a real HWND permanently" | True, and irrelevant — it has its own, and keeps it under either shape. |
| "cannot be engineered away" | **The Windows side is engineered away by doing nothing.** The Linux side is a real question, and it was never about the HWND. |

**Only one form uses WebView2 in the whole of tiko.** `grep -rl CWebView2 src/` returns
`frmHelpCenter.bi`, `frmHelpCenter.inc`, `tiko.bas` (which includes it) and
`_check_app_layer.bas` (the ratchet, which names it to forbid it). 970 lines, one window.

---

## WHAT THE HELP CENTER ACTUALLY IS

This is the fact that dissolves the problem, and it is not in `d2-decision.md` anywhere.

**It renders LOCAL STATIC HTML THAT WE GENERATE OURSELVES.** 147 MB under
`settings/help/helpcenter`, four docsets produced by `C:\dev\HelpCenter`'s `helpgen`, loaded as
`file:///` URLs. It is not a browser, it browses nothing, and it reaches no network.

**The integration is shallow.** Two event handlers and one script call:

| what | why it exists |
| --- | --- |
| `NavigationCompleted` | know when a page finished, for focus |
| `NewWindowRequested` | keep target=_blank inside the pane rather than spawning Edge |
| `ExecuteScript` ×1 | fill the site's own `#q` search box and focus it |

**No host objects, no `PostWebMessage`, no bidirectional bridge.** Nothing is asking WebView2 to
be an application platform. It is a viewer for HTML we wrote.

---

## SO THE REAL QUESTION

Not "how do we keep WebView2 under Shape A" — it keeps itself. The question is:

**What shows the documentation on Linux?** WebView2 is Windows-only; there is no port and there
will not be one.

### The options

**(a) A second engine — WebKitGTK.** A browser engine as a dependency, a second code path, and
a second set of bugs, to render static HTML we control. Rejected: it is the most expensive
answer to the least demanding content.

**(b) The user's default browser, via `SDL_OpenURL`.** Already in the binding
(`src/bind/SDL3/SDL3/SDL_misc.bi`), cross-platform, one call. The docs open in the browser the
user already has, with search, zoom, history, printing and bookmarks for free — all of which the
embedded pane lacks today.

**(c) WebView2 on Windows, browser everywhere else.** The Windows path exists and works; the
other platforms get (b).

**(d) Drop the Help Center off Windows.** Rejected: the docs are a third of the product's value
to a beginner, and "no documentation on Linux" is not a port.

### The recommendation

**Build (b), and treat the embedded pane as a WINDOWS-ONLY CAPABILITY on top of it** — which is
exactly how `Platform.bi` already models things like `bShellIntegration`.

That inverts the current framing in the useful direction. Today WebView2 is load-bearing and
therefore a constraint. Under this answer the *portable* path is the primary one, WebView2 is an
enhancement, and **it can be deleted on any day it becomes inconvenient without the Help Center
going with it.** A dependency you are free to drop is not a blocker.

---

## THE ONE THING THAT IS NOT FREE

The `ExecuteScript` call. F1 on a symbol opens the Help Center with that symbol already typed
into the site's search box, and a browser launched at a `file:///` URL cannot be told to do that
from outside.

**The fix is a URL query parameter, and it belongs in `helpgen`, not in tiko.** The site is ours.
`assets/app.js` already reads `URLSearchParams` — for `?theme=` — so the machinery is there and
the change is to honour `?q=` beside it. Then F1 becomes:

```
file:///…/helpcenter/index.html?q=CreateWindowEx
```

and the injection disappears from tiko entirely, on Windows too. **That is a small change in a
repository we own, and it removes a tiko↔WebView2 coupling rather than porting one.**

Checked rather than assumed: `app.js` reads `?theme=` today and **does not** read `?q=`. So this
is real work, not a claim that it already works.

---

## WHAT THIS MEANS FOR D2

**WebView2 is off the critical path. It constrains neither shape.**

* **Shape A** (frmMain becomes a `PsSurface`): `frmHelpCenter` stays a plain Win32 top-level
  window with its WebView2 child, opened from the SDL3 main window. Two window systems in one
  process, which is unusual but not novel — tiko already runs a Win32 shell around a
  Blend2D-rendered editor. On Linux the same menu item opens the browser.
* **Shape B** (`PsWin32Host` promoted): nothing changes at all.

`d2-decision.md` said this question "deserves to drive the answer rather than be discovered
halfway". It has now been asked, and **it does not drive the answer** — which is worth as much
as if it had, because it was the last thing standing between D2 and a decision.

**D2 is now unblocked in every direction its own page identified.** What remains is a judgement
about a second `IWindowBackend` versus a 45,000-line jump, taken against evidence that all three
prerequisites and this page now supply.

---

## HOW THIS WAS CHECKED

Every claim above came from the code, not from memory of it:

```
grep -rl "CWebView2" src/                  # one form, plus the include and the ratchet
grep -n "WS_POPUP" src/frmHelpCenter.inc   # a top-level window, not a pane
grep -n "file:///" src/frmHelpCenter.inc   # local content
grep -rn "SDL_PROP_WINDOW_WIN32_HWND"      # SDL3 does hand over the HWND
grep -rn "SDL_OpenURL"                     # and can open the default browser
grep -o "URLSearchParams[^;]*" …/app.js    # ?theme= yes, ?q= no
```

**Nothing here has been built or tested.** This is an investigation and a recommendation; the
`?q=` change in `helpgen` and the `SDL_OpenURL` path in `IPlatform` are both unwritten.
