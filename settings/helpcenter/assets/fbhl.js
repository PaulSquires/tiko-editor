/* A small FreeBASIC syntax highlighter for signatures and examples.
 * Runs over <pre class="code"> at load; no external highlighter, no build step. */
(function () {
  "use strict";

  var KEYWORDS = ("function sub property constructor destructor operator declare private public " +
    "static shared const dim as byval byref any ptr pointer type union enum end namespace using " +
    "if then else elseif select case with do loop while wend for next to step exit continue " +
    "return goto gosub scope extern export import lib alias overload cdecl stdcall pascal " +
    "and or xor not andalso orelse mod shl shr imp eqv is new delete cast cptr sizeof varptr " +
    "procptr strptr this base extends abstract virtual implements let get set option preserve " +
    "redim erase swap print open close line input output append random binary common " +
    "true false null nothing byte ubyte short ushort long ulong integer uinteger longint " +
    "ulongint single double string wstring zstring boolean").split(" ");

  var TYPES = ("hwnd hdc hresult dword word bool lresult wparam lparam uint int_ lpstr lpwstr " +
    "hinstance hicon hbrush hpen hfont hmenu hbitmap rect point size msg dwstring bstring " +
    "colorref argb real gpstatus variant safearray ansistring").split(" ");

  var kw = Object.create(null), ty = Object.create(null);
  KEYWORDS.forEach(function (k) { kw[k] = 1; });
  TYPES.forEach(function (t) { ty[t] = 1; });

  function esc(s) {
    return s.replace(/[&<>]/g, function (c) { return { "&": "&amp;", "<": "&lt;", ">": "&gt;" }[c]; });
  }

  function highlight(src) {
    var out = "";
    var i = 0;
    while (i < src.length) {
      var c = src.charAt(i);

      // Comment: ' to end of line, and the REM form.
      if (c === "'") {
        var nl = src.indexOf("\n", i);
        if (nl < 0) nl = src.length;
        out += '<span class="tok-cm">' + esc(src.slice(i, nl)) + "</span>";
        i = nl;
        continue;
      }
      // String literal.
      if (c === '"') {
        var j = i + 1;
        while (j < src.length && src.charAt(j) !== '"' && src.charAt(j) !== "\n") j++;
        if (src.charAt(j) === '"') j++;
        out += '<span class="tok-st">' + esc(src.slice(i, j)) + "</span>";
        i = j;
        continue;
      }
      // Number, including &H hex.
      if (/[0-9]/.test(c) || (c === "&" && /[hHoObB]/.test(src.charAt(i + 1)))) {
        var k = i;
        if (c === "&") k += 2;
        while (k < src.length && /[0-9a-fA-F.]/.test(src.charAt(k))) k++;
        out += '<span class="tok-nu">' + esc(src.slice(i, k)) + "</span>";
        i = k;
        continue;
      }
      // Identifier.
      if (/[A-Za-z_]/.test(c)) {
        var m = i;
        while (m < src.length && /[A-Za-z0-9_]/.test(src.charAt(m))) m++;
        var word = src.slice(i, m), lower = word.toLowerCase();
        if (kw[lower]) out += '<span class="tok-kw">' + esc(word) + "</span>";
        else if (ty[lower]) out += '<span class="tok-ty">' + esc(word) + "</span>";
        else out += esc(word);
        i = m;
        continue;
      }
      out += esc(c);
      i++;
    }
    return out;
  }

  function run() {
    var blocks = document.querySelectorAll("pre.code > code");
    for (var i = 0; i < blocks.length; i++) {
      var el = blocks[i];
      if (el.dataset.hl) continue;
      el.dataset.hl = "1";
      el.innerHTML = highlight(el.textContent);
    }
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", run);
  else run();
})();
