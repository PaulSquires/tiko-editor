/* ==========================================================================
   Tiko Editor — Help System
   Shared behaviour. Vanilla ES5-compatible JavaScript, no dependencies.

   Runs from file:// — nothing here uses fetch(), XHR or modules, because
   browsers block those for local files. The search index is loaded as a
   plain <script> that assigns window.TIKO_SEARCH_INDEX.
   ========================================================================== */
(function () {
  'use strict';

  var STORE_THEME = 'tiko-help-theme';
  var STORE_NAV = 'tiko-help-nav';

  function $(sel, root) { return (root || document).querySelector(sel); }
  function $$(sel, root) {
    return Array.prototype.slice.call((root || document).querySelectorAll(sel));
  }

  /* ------------------------------------------------------------------ */
  /* 1. Theme                                                            */
  /* ------------------------------------------------------------------ */

  function storedTheme() {
    try { return localStorage.getItem(STORE_THEME); } catch (e) { return null; }
  }

  function applyTheme(theme) {
    document.documentElement.setAttribute('data-theme', theme);
    var btn = $('#theme-toggle');
    if (btn) {
      var dark = theme === 'dark';
      btn.setAttribute('aria-label', dark ? 'Switch to light theme' : 'Switch to dark theme');
      btn.setAttribute('title', dark ? 'Switch to light theme' : 'Switch to dark theme');
      var sun = $('.icon-sun', btn), moon = $('.icon-moon', btn);
      if (sun) sun.style.display = dark ? 'none' : '';
      if (moon) moon.style.display = dark ? '' : 'none';
    }
  }

  function initTheme() {
    var saved = storedTheme();
    var theme = saved || (window.matchMedia &&
      window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light');
    applyTheme(theme);

    var btn = $('#theme-toggle');
    if (!btn) return;
    btn.addEventListener('click', function () {
      var next = document.documentElement.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
      applyTheme(next);
      try { localStorage.setItem(STORE_THEME, next); } catch (e) {}
    });

    if (window.matchMedia) {
      var mq = window.matchMedia('(prefers-color-scheme: dark)');
      var onChange = function (e) {
        if (!storedTheme()) applyTheme(e.matches ? 'dark' : 'light');
      };
      if (mq.addEventListener) mq.addEventListener('change', onChange);
      else if (mq.addListener) mq.addListener(onChange);
    }
  }

  /* ------------------------------------------------------------------ */
  /* 2. Sidebar navigation                                               */
  /* ------------------------------------------------------------------ */

  function readNavState() {
    try { return JSON.parse(localStorage.getItem(STORE_NAV) || '{}'); }
    catch (e) { return {}; }
  }

  function writeNavState(state) {
    try { localStorage.setItem(STORE_NAV, JSON.stringify(state)); } catch (e) {}
  }

  function initNav() {
    var state = readNavState();

    $$('.nav-section-btn').forEach(function (btn) {
      var id = btn.getAttribute('data-section');
      var body = btn.nextElementSibling;
      var hasCurrent = body && body.querySelector('[aria-current="page"]');

      // A section holding the current page is always opened, whatever was stored.
      var expanded = hasCurrent ? true
        : (Object.prototype.hasOwnProperty.call(state, id) ? !!state[id]
          : btn.getAttribute('aria-expanded') !== 'false');

      btn.setAttribute('aria-expanded', expanded ? 'true' : 'false');

      btn.addEventListener('click', function () {
        var open = btn.getAttribute('aria-expanded') === 'true';
        btn.setAttribute('aria-expanded', open ? 'false' : 'true');
        state[id] = !open;
        writeNavState(state);
      });
    });

    // Mobile drawer
    var toggle = $('#nav-toggle');
    var sidebar = $('#sidebar');
    var scrim = $('#nav-scrim');
    if (!toggle || !sidebar) return;

    function setDrawer(open) {
      sidebar.classList.toggle('open', open);
      if (scrim) scrim.classList.toggle('open', open);
      toggle.setAttribute('aria-expanded', open ? 'true' : 'false');
    }

    toggle.addEventListener('click', function () {
      setDrawer(!sidebar.classList.contains('open'));
    });
    if (scrim) scrim.addEventListener('click', function () { setDrawer(false); });
    sidebar.addEventListener('click', function (e) {
      if (e.target.closest('a')) setDrawer(false);
    });
    document.addEventListener('keydown', function (e) {
      if (e.key === 'Escape' && sidebar.classList.contains('open')) {
        setDrawer(false);
        toggle.focus();
      }
    });

    // Keep the active item in view inside a long tree.
    var current = sidebar.querySelector('[aria-current="page"]');
    if (current && current.scrollIntoView) {
      var top = current.offsetTop - sidebar.clientHeight / 2;
      if (top > 0) sidebar.scrollTop = top;
    }
  }

  /* ------------------------------------------------------------------ */
  /* 3. Syntax highlighting                                              */
  /*                                                                     */
  /* Deliberately small: enough to make examples readable, not a full     */
  /* language parser. Operates on textContent, so nothing in the source   */
  /* can inject markup.                                                   */
  /* ------------------------------------------------------------------ */

  var GRAMMARS = {
    fb: {
      lineComment: ["'"],
      comment: true,
      keywords: ('dim as sub function end if then else elseif for next do loop while wend ' +
        'select case exit continue return byval byref const shared static var redim preserve ' +
        'type union enum declare private public scope with to step until any ptr pointer ' +
        'and or xor not andalso orelse mod imp eqv new delete cast operator property ' +
        'constructor constructor destructor extends implements namespace using overload ' +
        'print open close line input write get put screen sleep cls color randomize ' +
        'option explicit goto gosub let is next each throw try catch finally').split(' '),
      types: ('integer long longint short byte ubyte ushort uinteger ulong ulongint single ' +
        'double string zstring wstring boolean any object variant dwstring hwnd hdc rect').split(' '),
      preproc: /^\s*#\w+/,
      builtins: ('len left right mid instr trim ltrim rtrim ucase lcase str val chr asc ' +
        'iif abs sgn int fix sqr sin cos tan atn exp log rnd allocate deallocate callocate').split(' ')
    },
    c: {
      lineComment: ['//'],
      block: ['/*', '*/'],
      keywords: ('auto break case char const continue default do double else enum extern ' +
        'float for goto if inline int long register restrict return short signed sizeof ' +
        'static struct switch typedef union unsigned void volatile while class public ' +
        'private protected namespace template new delete this nullptr true false bool').split(' '),
      types: ('size_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t ' +
        'HWND HDC LRESULT WPARAM LPARAM BOOL DWORD UINT LPCWSTR wchar_t FILE').split(' '),
      preproc: /^\s*#\w+/,
      builtins: ('printf sprintf malloc free memcpy memset strlen strcpy fopen fclose').split(' ')
    },
    ini: { ini: true },
    text: {}
  };

  function escapeHtml(s) {
    return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }

  function highlightLine(line, lang) {
    var g = GRAMMARS[lang] || GRAMMARS.text;
    if (!g || (!g.keywords && !g.ini)) return escapeHtml(line);

    // INI / settings files
    if (g.ini) {
      if (/^\s*[;'#]/.test(line)) return '<span class="tok-com">' + escapeHtml(line) + '</span>';
      var sect = line.match(/^\s*(\[[^\]]*\])\s*$/);
      if (sect) return escapeHtml(line).replace(escapeHtml(sect[1]),
        '<span class="tok-kw">' + escapeHtml(sect[1]) + '</span>');
      var kv = line.match(/^(\s*)([^=]+)(=)(.*)$/);
      if (kv) {
        return kv[1] + '<span class="tok-fn">' + escapeHtml(kv[2]) + '</span>' +
          '<span class="tok-op">=</span>' +
          '<span class="tok-str">' + escapeHtml(kv[4]) + '</span>';
      }
      return escapeHtml(line);
    }

    var out = '';
    var i = 0;
    var n = line.length;

    while (i < n) {
      var rest = line.slice(i);

      // Comments run to end of line
      var isComment = false;
      if (g.lineComment) {
        for (var c = 0; c < g.lineComment.length; c++) {
          if (rest.indexOf(g.lineComment[c]) === 0) { isComment = true; break; }
        }
      }
      if (!isComment && lang === 'fb' && /^rem\b/i.test(rest)) isComment = true;
      if (isComment) {
        out += '<span class="tok-com">' + escapeHtml(rest) + '</span>';
        break;
      }

      // Strings
      var ch = line.charAt(i);
      if (ch === '"' || (lang === 'c' && ch === "'")) {
        var end = i + 1;
        while (end < n) {
          if (line.charAt(end) === '\\' && lang === 'c') { end += 2; continue; }
          if (line.charAt(end) === ch) { end++; break; }
          end++;
        }
        out += '<span class="tok-str">' + escapeHtml(line.slice(i, end)) + '</span>';
        i = end;
        continue;
      }

      // Preprocessor at line start
      if (g.preproc && i === 0) {
        var pp = rest.match(g.preproc);
        if (pp) {
          out += '<span class="tok-pp">' + escapeHtml(pp[0]) + '</span>';
          i += pp[0].length;
          continue;
        }
      }

      // Words
      var w = rest.match(/^[A-Za-z_][A-Za-z0-9_]*/);
      if (w) {
        var word = w[0];
        var lower = word.toLowerCase();
        var cls = null;
        if (g.keywords && g.keywords.indexOf(lower) !== -1) cls = 'tok-kw';
        else if (g.types && g.types.indexOf(lower) !== -1) cls = 'tok-type';
        else if (g.builtins && g.builtins.indexOf(lower) !== -1) cls = 'tok-fn';
        else if (/^\s*\(/.test(rest.slice(word.length))) cls = 'tok-fn';

        out += cls ? '<span class="' + cls + '">' + escapeHtml(word) + '</span>' : escapeHtml(word);
        i += word.length;
        continue;
      }

      // Numbers
      var num = rest.match(/^(&[hHoObB][0-9A-Fa-f]+|0[xX][0-9A-Fa-f]+|\d+(\.\d+)?)/);
      if (num) {
        out += '<span class="tok-num">' + escapeHtml(num[0]) + '</span>';
        i += num[0].length;
        continue;
      }

      // Operators
      if ('+-*/\\<>=(),:&|^.[]{};'.indexOf(ch) !== -1) {
        out += '<span class="tok-op">' + escapeHtml(ch) + '</span>';
        i++;
        continue;
      }

      out += escapeHtml(ch);
      i++;
    }

    return out;
  }

  function initCode() {
    $$('.code-block').forEach(function (block) {
      var codeEl = $('code', block);
      if (!codeEl) return;

      var lang = block.getAttribute('data-lang') || 'text';
      var source = codeEl.textContent.replace(/\s+$/, '');
      var lines = source.split('\n');
      var inBlockComment = false;
      var g = GRAMMARS[lang] || {};

      var html = lines.map(function (line, idx) {
        var body;
        // Simple /* */ and /' '/ block-comment tracking
        if (inBlockComment) {
          body = '<span class="tok-com">' + escapeHtml(line) + '</span>';
          if (line.indexOf('*/') !== -1 || line.indexOf("'/") !== -1) inBlockComment = false;
        } else if ((g.block && line.indexOf('/*') !== -1 && line.indexOf('*/') === -1) ||
                   (lang === 'fb' && line.indexOf("/'") !== -1 && line.indexOf("'/") === -1)) {
          inBlockComment = true;
          body = '<span class="tok-com">' + escapeHtml(line) + '</span>';
        } else {
          body = highlightLine(line, lang);
        }
        return '<span class="code-line" data-line="' + (idx + 1) + '">' +
          (body === '' ? '&nbsp;' : body) + '</span>';
      }).join('\n');

      codeEl.innerHTML = html;

      // Copy button
      var btn = $('.copy-btn', block);
      if (btn) {
        btn.addEventListener('click', function () {
          var done = function () {
            btn.classList.add('copied');
            var label = $('.copy-label', btn);
            var old = label ? label.textContent : '';
            if (label) label.textContent = 'Copied';
            setTimeout(function () {
              btn.classList.remove('copied');
              if (label) label.textContent = old || 'Copy';
            }, 1600);
          };
          if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(source).then(done, function () {});
          } else {
            var ta = document.createElement('textarea');
            ta.value = source;
            ta.style.position = 'fixed';
            ta.style.opacity = '0';
            document.body.appendChild(ta);
            ta.select();
            try { document.execCommand('copy'); done(); } catch (e) {}
            document.body.removeChild(ta);
          }
        });
      }
    });
  }

  /* ------------------------------------------------------------------ */
  /* 4. On-page table of contents + scrollspy                            */
  /* ------------------------------------------------------------------ */

  function initToc() {
    var toc = $('#page-toc');
    var content = $('#content');
    if (!toc || !content) return;

    var headings = $$('h2[id], h3[id]', content);
    if (headings.length < 2) { toc.style.display = 'none'; return; }

    var list = document.createElement('ul');
    headings.forEach(function (h) {
      var li = document.createElement('li');
      var a = document.createElement('a');
      a.href = '#' + h.id;
      a.textContent = h.textContent.replace('#', '').trim();
      if (h.tagName === 'H3') a.className = 'toc-h3';
      li.appendChild(a);
      list.appendChild(li);
    });
    var title = document.createElement('div');
    title.className = 'toc-title';
    title.textContent = 'On this page';
    toc.appendChild(title);
    toc.appendChild(list);

    var links = $$('a', list);

    function spy() {
      var pos = window.scrollY + parseInt(getComputedStyle(document.documentElement)
        .getPropertyValue('--header-h'), 10) + 40;
      var activeIdx = 0;
      for (var i = 0; i < headings.length; i++) {
        if (headings[i].offsetTop <= pos) activeIdx = i;
      }
      links.forEach(function (l, i) { l.classList.toggle('active', i === activeIdx); });
    }

    var ticking = false;
    window.addEventListener('scroll', function () {
      if (ticking) return;
      ticking = true;
      window.requestAnimationFrame(function () { spy(); ticking = false; });
    }, { passive: true });
    spy();
  }

  /* Heading anchor links */
  function initAnchors() {
    var content = $('#content');
    if (!content) return;
    $$('h2[id], h3[id]', content).forEach(function (h) {
      var a = document.createElement('a');
      a.className = 'heading-anchor';
      a.href = '#' + h.id;
      a.setAttribute('aria-label', 'Link to this section');
      a.textContent = '#';
      h.appendChild(a);
    });
  }

  /* ------------------------------------------------------------------ */
  /* 5. Back to top                                                      */
  /* ------------------------------------------------------------------ */

  function initBackToTop() {
    var btn = $('#back-to-top');
    if (!btn) return;
    btn.addEventListener('click', function () {
      window.scrollTo({ top: 0, behavior: 'smooth' });
      var h1 = $('#content h1');
      if (h1) { h1.setAttribute('tabindex', '-1'); h1.focus({ preventScroll: true }); }
    });
    var onScroll = function () {
      btn.classList.toggle('visible', window.scrollY > 500);
    };
    window.addEventListener('scroll', onScroll, { passive: true });
    onScroll();
  }

  /* ------------------------------------------------------------------ */
  /* 6. Search                                                           */
  /* ------------------------------------------------------------------ */

  function tokenize(s) {
    return s.toLowerCase().split(/[^a-z0-9+#]+/).filter(function (t) { return t.length > 0; });
  }

  function scoreEntry(entry, terms) {
    var score = 0;
    var title = entry.t.toLowerCase();
    var keys = (entry.k || '').toLowerCase();
    var body = (entry.b || '').toLowerCase();

    for (var i = 0; i < terms.length; i++) {
      var term = terms[i];
      var hit = 0;
      if (title.indexOf(term) !== -1) {
        hit += 60;
        if (title.indexOf(term) === 0) hit += 25;
        if (title === term) hit += 60;
      }
      if (keys.indexOf(term) !== -1) hit += 28;
      var pos = body.indexOf(term);
      if (pos !== -1) {
        hit += 12;
        var occurrences = body.split(term).length - 1;
        hit += Math.min(occurrences, 6);
      }
      if (hit === 0) return 0;   // every term must appear somewhere
      score += hit;
    }
    return score;
  }

  function makeSnippet(entry, terms) {
    var body = entry.b || '';
    if (!body) return '';
    var lower = body.toLowerCase();
    var at = -1;
    for (var i = 0; i < terms.length && at === -1; i++) at = lower.indexOf(terms[i]);
    if (at === -1) at = 0;

    var start = Math.max(0, at - 60);
    var text = body.slice(start, start + 200);
    if (start > 0) text = '…' + text;
    if (start + 200 < body.length) text += '…';

    // Escape, then wrap matches
    var html = text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    terms.forEach(function (t) {
      if (!t) return;
      var safe = t.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
      html = html.replace(new RegExp('(' + safe + ')', 'gi'), '<mark>$1</mark>');
    });
    return html;
  }

  function initSearch() {
    var overlay = $('#search-overlay');
    var input = $('#search-input');
    var results = $('#search-results');
    var launcher = $('#search-launcher');
    if (!overlay || !input || !results) return;

    var index = window.TIKO_SEARCH_INDEX || [];
    var base = document.documentElement.getAttribute('data-base') || '';
    var activeIdx = -1;
    var currentLinks = [];

    function open() {
      overlay.classList.add('open');
      document.body.style.overflow = 'hidden';
      input.focus();
      input.select();
      if (!input.value) renderHint();
    }

    function close() {
      overlay.classList.remove('open');
      document.body.style.overflow = '';
      if (launcher) launcher.focus();
    }

    function renderHint() {
      results.innerHTML = '';
      var li = document.createElement('li');
      li.className = 'search-hintrow';
      li.innerHTML = 'Search ' + index.length + ' topics. Try <code>breakpoint</code>, ' +
        '<code>theme</code>, <code>find in project</code> or <code>F5</code>.';
      results.appendChild(li);
      currentLinks = [];
      activeIdx = -1;
    }

    function render(query) {
      var terms = tokenize(query);
      results.innerHTML = '';
      currentLinks = [];
      activeIdx = -1;

      if (!terms.length) { renderHint(); return; }

      var scored = [];
      for (var i = 0; i < index.length; i++) {
        var s = scoreEntry(index[i], terms);
        if (s > 0) scored.push({ e: index[i], s: s });
      }
      scored.sort(function (a, b) { return b.s - a.s; });
      scored = scored.slice(0, 25);

      if (!scored.length) {
        var empty = document.createElement('li');
        empty.className = 'search-empty';
        empty.textContent = 'No results for “' + query + '”.';
        results.appendChild(empty);
        return;
      }

      scored.forEach(function (item) {
        var li = document.createElement('li');
        var a = document.createElement('a');
        a.href = base + item.e.u + '?q=' + encodeURIComponent(query);
        a.innerHTML =
          '<span class="sr-crumb">' + item.e.s + '</span>' +
          '<span class="sr-title">' + item.e.t + '</span>' +
          '<span class="sr-snippet">' + makeSnippet(item.e, terms) + '</span>';
        li.appendChild(a);
        results.appendChild(li);
        currentLinks.push(a);
      });
    }

    function move(delta) {
      if (!currentLinks.length) return;
      if (activeIdx >= 0) currentLinks[activeIdx].classList.remove('active');
      activeIdx = (activeIdx + delta + currentLinks.length) % currentLinks.length;
      var a = currentLinks[activeIdx];
      a.classList.add('active');
      a.scrollIntoView({ block: 'nearest' });
    }

    if (launcher) launcher.addEventListener('click', open);

    overlay.addEventListener('click', function (e) {
      if (e.target === overlay) close();
    });

    var closeBtn = $('#search-close');
    if (closeBtn) closeBtn.addEventListener('click', close);

    var debounce;
    input.addEventListener('input', function () {
      clearTimeout(debounce);
      var v = input.value;
      debounce = setTimeout(function () { render(v); }, 90);
    });

    input.addEventListener('keydown', function (e) {
      if (e.key === 'ArrowDown') { e.preventDefault(); move(1); }
      else if (e.key === 'ArrowUp') { e.preventDefault(); move(-1); }
      else if (e.key === 'Enter') {
        if (activeIdx >= 0) { e.preventDefault(); currentLinks[activeIdx].click(); }
        else if (currentLinks.length) { e.preventDefault(); currentLinks[0].click(); }
      }
    });

    document.addEventListener('keydown', function (e) {
      var tag = (e.target.tagName || '').toLowerCase();
      var typing = tag === 'input' || tag === 'textarea' || e.target.isContentEditable;

      if (e.key === 'Escape' && overlay.classList.contains('open')) { close(); return; }
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'k') {
        e.preventDefault(); open(); return;
      }
      if (e.key === '/' && !typing && !e.ctrlKey && !e.metaKey && !e.altKey) {
        e.preventDefault(); open();
      }
    });
  }

  /* Highlight ?q= terms in the page body after arriving from a search. */
  function initQueryHighlight() {
    var match = window.location.search.match(/[?&]q=([^&]*)/);
    if (!match) return;
    var query = decodeURIComponent(match[1].replace(/\+/g, ' '));
    var terms = tokenize(query);
    if (!terms.length) return;

    var content = $('#content');
    if (!content) return;

    var walker = document.createTreeWalker(content, NodeFilter.SHOW_TEXT, {
      acceptNode: function (node) {
        if (!node.nodeValue.trim()) return NodeFilter.FILTER_REJECT;
        var p = node.parentNode;
        while (p && p !== content) {
          var t = p.tagName;
          if (t === 'SCRIPT' || t === 'STYLE' || t === 'CODE' || t === 'PRE' || t === 'MARK') {
            return NodeFilter.FILTER_REJECT;
          }
          p = p.parentNode;
        }
        return NodeFilter.FILTER_ACCEPT;
      }
    });

    var nodes = [];
    var node;
    while ((node = walker.nextNode())) nodes.push(node);

    var pattern = new RegExp('(' + terms.map(function (t) {
      return t.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    }).join('|') + ')', 'gi');

    var first = null;
    nodes.forEach(function (n) {
      var text = n.nodeValue;
      if (!pattern.test(text)) return;
      pattern.lastIndex = 0;

      var frag = document.createDocumentFragment();
      var last = 0, m;
      while ((m = pattern.exec(text)) !== null) {
        if (m.index > last) frag.appendChild(document.createTextNode(text.slice(last, m.index)));
        var mark = document.createElement('mark');
        mark.className = 'search-hit';
        mark.textContent = m[0];
        frag.appendChild(mark);
        if (!first) first = mark;
        last = m.index + m[0].length;
      }
      if (last < text.length) frag.appendChild(document.createTextNode(text.slice(last)));
      n.parentNode.replaceChild(frag, n);
    });

    if (first && !window.location.hash) {
      setTimeout(function () {
        first.scrollIntoView({ block: 'center', behavior: 'smooth' });
      }, 120);
    }
  }

  /* ------------------------------------------------------------------ */
  /* 7. Boot                                                             */
  /* ------------------------------------------------------------------ */

  function boot() {
    initTheme();
    initNav();
    initCode();
    initAnchors();
    initToc();
    initBackToTop();
    initSearch();
    initQueryHighlight();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', boot);
  } else {
    boot();
  }
})();
