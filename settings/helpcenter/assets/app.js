/* HelpCenter — search, tag filtering, theming. No framework, no build step.
 *
 * The index arrives as `window.HVIDX` from search-index.js, loaded with a plain
 * <script src>. That is deliberate: fetch() is CORS-blocked on file://, so a page opened
 * straight off disk (or inside tiko's WebView2 pane) could not load a JSON index at all.
 *
 * Every page sets window.HV_ROOT to its own depth-relative path back to the site root,
 * because no URL here may be root-relative — the same build has to work at file:///…,
 * inside WebView2, and at https://www.planetsquires.com/docs/.
 */
(function () {
  "use strict";

  var ROOT = window.HV_ROOT || "";
  var IDX = window.HVIDX || { rows: [], docsets: {} };
  var rows = IDX.rows || [];

  /* Row layout, mirrored in searchindex.py:
     0 name  1 qualified  2 owner  3 kind  4 category  5 status  6 summary  7 url  8 docset
     9 external */
  var N = 0, Q = 1, OWN = 2, KIND = 3, CAT = 4, ST = 5, SUM = 6, URL = 7, DS = 8, EXT = 9;

  /* Where a search hit points. Rows carrying an `external` URL (the Win32 docset) link
     straight to the upstream documentation in a new tab rather than to the local launcher
     page: the click is a user gesture, so the tab always opens, whereas the launcher's own
     window.open is at the mercy of the popup blocker. Older indexes have no column 9, hence
     the guard — a stale cached search-index.js must not break search entirely. */
  function linkAttrs(r) {
    var ext = r.length > EXT ? r[EXT] : "";
    if (ext) return { href: ext, target: ' target="_blank" rel="noopener noreferrer"' };
    return { href: ROOT + r[URL], target: "" };
  }

  /* ------------------------------------------------------------------ theme */
  var THEME_KEY = "hv-theme";
  function applyTheme(t) {
    if (t === "dark" || t === "light") document.documentElement.setAttribute("data-theme", t);
    else document.documentElement.removeAttribute("data-theme");
  }
  // ?theme=dark lets tiko force the editor's theme into the embedded pane. The hash form
  // (#theme=dark) is accepted too: some hosts drop the query string from a file:// URL —
  // this preview pane does — and a fragment always survives navigation.
  var forced = new URLSearchParams(location.search).get("theme") ||
               new URLSearchParams(location.hash.replace(/^#/, "")).get("theme");
  if (forced) { applyTheme(forced); try { localStorage.setItem(THEME_KEY, forced); } catch (e) {} }
  else { try { applyTheme(localStorage.getItem(THEME_KEY)); } catch (e) {} }

  function toggleTheme() {
    var cur = document.documentElement.getAttribute("data-theme");
    if (!cur) {
      cur = window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
    }
    var next = cur === "dark" ? "light" : "dark";
    applyTheme(next);
    try { localStorage.setItem(THEME_KEY, next); } catch (e) {}
  }

  /* --------------------------------------------------------------- ranking */

  // "asc" should find AfxStrClipLeft: the capitals of a name, plus its first letter.
  var humpCache = Object.create(null);
  function humps(name) {
    var h = humpCache[name];
    if (h !== undefined) return h;
    var out = name.charAt(0);
    for (var i = 1; i < name.length; i++) {
      var c = name.charAt(i);
      if (c >= "A" && c <= "Z") out += c;
      else if (c === "_" && i + 1 < name.length) out += name.charAt(i + 1);
    }
    h = out.toLowerCase();
    humpCache[name] = h;
    return h;
  }

  function subsequence(hay, needle) {
    var i = 0;
    for (var j = 0; j < hay.length && i < needle.length; j++) {
      if (hay.charAt(j) === needle.charAt(i)) i++;
    }
    return i === needle.length;
  }

  function score(row, q) {
    var name = row[N].toLowerCase();
    var qual = row[Q].toLowerCase();
    var s = -1;

    var at = name.indexOf(q);
    if (name === q) s = 1000;
    else if (qual === q) s = 950;
    else if (at === 0) s = 800;
    else if (qual.indexOf(q) === 0) s = 700;
    // A substring starting on a word boundary is a more literal hit than an
    // initials match, so `mid` finds AfxStrClip|Mid before Map|I|D|ToIndex.
    else if (at > 0 && /[A-Z_]/.test(row[N].charAt(at))) s = 650;
    else if (humps(row[N]).indexOf(q) === 0) s = 600;
    else if (at > 0) s = 400;
    else if (qual.indexOf(q) > 0) s = 300;
    else if (q.length >= 3 && subsequence(name, q)) s = 200;
    else if (q.length >= 3 && row[SUM].toLowerCase().indexOf(q) >= 0) s = 100;
    else return -1;

    // Shorter names are the better answer for the same prefix (`Add` over `AddControl`).
    s -= Math.min(name.length, 60) * 0.5;
    if (row[ST] === "documented") s += 30;
    else if (row[ST] === "undocumented") s -= 15;
    else if (row[ST] === "ai-drafted") s -= 10;
    return s;
  }

  /* --------------------------------------------------------------- filters */
  var active = Object.create(null);   // facet -> Set of selected values

  function facetOf(row, facet) {
    if (facet === "docset") return IDX.docsets && IDX.docsets[row[DS]] ? IDX.docsets[row[DS]] : row[DS];
    if (facet === "kind") return row[KIND];
    if (facet === "category") return row[CAT];
    if (facet === "status") return row[ST];
    return "";
  }

  // AND across facets, OR within one: two categories widen, a category plus a kind narrows.
  function passes(row) {
    for (var facet in active) {
      var sel = active[facet];
      if (!sel || !sel.size) continue;
      if (!sel.has(facetOf(row, facet))) return false;
    }
    return true;
  }

  /* ----------------------------------------------------------------- view */
  var input = document.getElementById("q");
  var panel = document.getElementById("results");
  if (!input || !panel) return;

  var current = [];
  var cursor = -1;

  function esc(s) {
    return String(s).replace(/[&<>"]/g, function (c) {
      return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c];
    });
  }

  function highlight(name, q) {
    var i = name.toLowerCase().indexOf(q);
    if (i < 0 || !q) return esc(name);
    return esc(name.slice(0, i)) + "<mark>" + esc(name.slice(i, i + q.length)) +
           "</mark>" + esc(name.slice(i + q.length));
  }

  function facetCounts(hits, facet) {
    var counts = Object.create(null);
    for (var i = 0; i < hits.length; i++) {
      var v = facetOf(hits[i], facet);
      if (v) counts[v] = (counts[v] || 0) + 1;
    }
    return counts;
  }

  function renderFacets(preFilter) {
    var facets = ["docset", "kind", "category", "status"];
    var html = "";
    for (var f = 0; f < facets.length; f++) {
      var facet = facets[f];
      // A facet's own selection must not shrink its own option list.
      var pool = preFilter.filter(function (row) {
        for (var other in active) {
          if (other === facet) continue;
          var sel = active[other];
          if (sel && sel.size && !sel.has(facetOf(row, other))) return false;
        }
        return true;
      });
      var counts = facetCounts(pool, facet);
      var keys = Object.keys(counts).sort(function (a, b) { return counts[b] - counts[a]; });
      if (facet === "docset" && keys.length < 2) continue;   // pointless with one docset
      for (var k = 0; k < keys.length && k < 8; k++) {
        var on = active[facet] && active[facet].has(keys[k]);
        html += '<button class="chip" data-facet="' + facet + '" data-value="' + esc(keys[k]) +
                '" aria-pressed="' + (on ? "true" : "false") + '">' + esc(keys[k]) +
                '<span class="chip__n">' + counts[keys[k]] + "</span></button>";
      }
    }
    return html;
  }

  function render(q) {
    if (!q) { panel.hidden = true; panel.innerHTML = ""; current = []; cursor = -1; return; }

    var scored = [];
    for (var i = 0; i < rows.length; i++) {
      var s = score(rows[i], q);
      if (s > 0) scored.push([s, rows[i]]);
    }
    scored.sort(function (a, b) { return b[0] - a[0]; });
    var preFilter = scored.map(function (x) { return x[1]; });
    current = preFilter.filter(passes).slice(0, 60);
    cursor = current.length ? 0 : -1;

    var html = '<div class="results__facets">' + renderFacets(preFilter) + "</div>";
    if (!current.length) {
      html += '<div class="results__empty">No matches' +
              (preFilter.length ? " with these filters" : " for “" + esc(q) + "”") + "</div>";
    } else {
      for (var j = 0; j < current.length; j++) {
        var r = current[j];
        var meta = [r[OWN] || r[CAT], r[KIND]].filter(Boolean).join(" · ");
        var link = linkAttrs(r);
        html += '<a class="hit" href="' + esc(link.href) + '"' + link.target +
          ' aria-selected="' + (j === cursor ? "true" : "false") + '">' +
          '<span class="hit__top"><span class="hit__name">' +
            (r[OWN] ? '<span style="opacity:.55">' + esc(r[OWN]) + ".</span>" : "") +
            highlight(r[N], q) + "</span>" +
          '<span class="hit__meta">' + esc(meta) + "</span></span>" +
          (r[SUM] ? '<span class="hit__sum">' + esc(r[SUM]) + "</span>" : "") +
          "</a>";
      }
    }
    panel.innerHTML = html;
    panel.hidden = false;
  }

  function moveCursor(delta) {
    var hits = panel.querySelectorAll(".hit");
    if (!hits.length) return;
    if (cursor >= 0 && hits[cursor]) hits[cursor].setAttribute("aria-selected", "false");
    cursor = (cursor + delta + hits.length) % hits.length;
    hits[cursor].setAttribute("aria-selected", "true");
    hits[cursor].scrollIntoView({ block: "nearest" });
  }

  input.addEventListener("input", function () { render(input.value.trim().toLowerCase()); });
  input.addEventListener("focus", function () {
    if (input.value.trim()) render(input.value.trim().toLowerCase());
  });

  panel.addEventListener("click", function (ev) {
    var chip = ev.target.closest ? ev.target.closest(".chip") : null;
    if (!chip) return;
    ev.preventDefault();
    var facet = chip.getAttribute("data-facet"), value = chip.getAttribute("data-value");
    if (!active[facet]) active[facet] = new Set();
    if (active[facet].has(value)) active[facet].delete(value);
    else active[facet].add(value);
    render(input.value.trim().toLowerCase());
    input.focus();
  });

  input.addEventListener("keydown", function (ev) {
    if (ev.key === "ArrowDown") { ev.preventDefault(); moveCursor(1); }
    else if (ev.key === "ArrowUp") { ev.preventDefault(); moveCursor(-1); }
    else if (ev.key === "Enter") {
      var hits = panel.querySelectorAll(".hit");
      if (cursor >= 0 && hits[cursor]) {
        ev.preventDefault();
        // Enter must honour the hit's own target, or a Win32 result opens over the docs
        // instead of beside them — the one thing keyboard and mouse must not disagree on.
        // This runs inside a keydown, so it counts as a user gesture and is not blocked.
        if (hits[cursor].target === "_blank") window.open(hits[cursor].href, "_blank", "noopener");
        else location.href = hits[cursor].href;
      }
    } else if (ev.key === "Escape") {
      if (panel.hidden) input.blur();
      else { panel.hidden = true; }
    }
  });

  document.addEventListener("click", function (ev) {
    if (!panel.contains(ev.target) && ev.target !== input) panel.hidden = true;
  });

  document.addEventListener("keydown", function (ev) {
    var typing = /^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement.tagName);
    if ((ev.key === "/" && !typing) || (ev.key.toLowerCase() === "k" && (ev.ctrlKey || ev.metaKey))) {
      ev.preventDefault();
      input.focus();
      input.select();
    }
  });

  /* ------------------------------------------------------------------ nav
   * Built here from nav.js rather than inlined into every page: 137 topic links on each
   * of ~26,500 pages was most of the site's bytes and most of the first upload. */
  (function buildNav() {
    var host = document.getElementById("side");
    if (!host || !window.HVNAV) return;
    var current = host.getAttribute("data-current") || "";
    var html = "";
    for (var d = 0; d < HVNAV.length; d++) {
      var ds = HVNAV[d];
      html += "<h2>" + escHtml(ds.title) + "</h2>";
      for (var i = 0; i < ds.cats.length; i++) {
        var cat = ds.cats[i];
        // A urlless category is a readme docset's flat document list: render the topics as
        // plain links under the docset title, with no "All entries" row and no nesting.
        if (!cat.url) {
          for (var k = 0; k < cat.topics.length; k++) {
            var rt = cat.topics[k];
            html += '<a class="side__flat" href="' + ROOT + rt.u + '"' +
                    (rt.u === current ? ' aria-current="page"' : "") + ">" +
                    escHtml(rt.t) + "</a>";
          }
          continue;
        }
        // Open the section containing this page. Member pages live under the category's
        // own directory but are not topics, so a prefix test is what gives them context.
        var catDir = cat.url.replace(/index\.html$/, "");
        var open = cat.url === current ||
                   current.indexOf(catDir) === 0 ||
                   cat.topics.some(function (t) { return t.u === current; });
        html += "<details" + (open ? " open" : "") + "><summary>" +
                escHtml(cat.name) + "</summary><div>";
        html += '<a href="' + ROOT + cat.url + '"' +
                (cat.url === current ? ' aria-current="page"' : "") + ">All entries</a>";
        for (var j = 0; j < cat.topics.length; j++) {
          var t = cat.topics[j];
          html += '<a href="' + ROOT + t.u + '"' +
                  (t.u === current ? ' aria-current="page"' : "") + ">" + escHtml(t.t) + "</a>";
        }
        html += "</div></details>";
      }
    }
    host.innerHTML = html;
    var cur = host.querySelector('[aria-current="page"]');
    if (cur) cur.scrollIntoView({ block: "center" });
  })();

  function escHtml(s) { return esc(s); }

  var themeBtn = document.getElementById("theme");
  if (themeBtn) themeBtn.addEventListener("click", toggleTheme);

  var sideBtn = document.getElementById("sidetoggle");
  if (sideBtn) sideBtn.addEventListener("click", function () {
    var side = document.querySelector(".side");
    if (side) side.classList.toggle("side--open");
  });
})();
