#!/usr/bin/env python3
"""
Tiko Editor Help System — static site generator.

Emits a fully self-contained offline documentation site: one HTML file per
topic, sharing assets/styles.css and assets/script.js, plus a client-side
search index at assets/search-index.js.

Run from this directory:      python build.py
Output goes to the parent:    C:\\dev\\tikohelp\\
"""

import html
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(os.path.join(HERE, ".."))

SITE_TITLE = "Tiko Editor Help"
SITE_TAGLINE = "Documentation"
VERSION_LABEL = "Tiko Editor documentation"

# --------------------------------------------------------------------------
# Icons (inline SVG, currentColor)
# --------------------------------------------------------------------------

ICONS = {
    "chevron": '<svg class="chev" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 6l4 4 4-4"/></svg>',
    "search": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" aria-hidden="true"><circle cx="9" cy="9" r="6"/><path d="M13.5 13.5L17 17"/></svg>',
    "menu": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" aria-hidden="true"><path d="M3 5h14M3 10h14M3 15h14"/></svg>',
    "sun": '<svg class="icon-sun" viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" aria-hidden="true"><circle cx="10" cy="10" r="3.6"/><path d="M10 1.5v2M10 16.5v2M18.5 10h-2M3.5 10h-2M15.9 4.1l-1.4 1.4M5.5 14.5l-1.4 1.4M15.9 15.9l-1.4-1.4M5.5 5.5L4.1 4.1"/></svg>',
    "moon": '<svg class="icon-moon" viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M16.5 12.4A7 7 0 017.6 3.5a7 7 0 108.9 8.9z"/></svg>',
    "top": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M10 16V5M5 10l5-5 5 5"/></svg>',
    "copy": '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="5.5" y="5.5" width="8" height="8" rx="1.5"/><path d="M10.5 3.5h-7a1 1 0 00-1 1v7"/></svg>',
    "close": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" aria-hidden="true"><path d="M5 5l10 10M15 5L5 15"/></svg>',
    # Callouts
    "note": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" aria-hidden="true"><circle cx="10" cy="10" r="7.5"/><path d="M10 9v5M10 6.2v.1"/></svg>',
    "tip": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M7.5 15h5M8 17.5h4M10 2.5a5 5 0 00-3 9v1.5h6V11.5a5 5 0 00-3-9z"/></svg>',
    "warning": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M10 3L1.8 16.5h16.4L10 3z"/><path d="M10 8v4M10 14.5v.1"/></svg>',
    "important": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" aria-hidden="true"><path d="M10 2.2l2.4 4.9 5.4.8-3.9 3.8.9 5.4-4.8-2.5-4.8 2.5.9-5.4L2.2 7.9l5.4-.8z"/></svg>',
    "todo": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="3" width="14" height="14" rx="2.5"/><path d="M6.8 10.2l2.2 2.2 4.2-4.6"/></svg>',
    "image": '<svg class="ph-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="4" width="18" height="16" rx="2"/><circle cx="8.5" cy="9.5" r="1.8"/><path d="M3 16.5l4.5-4 3.5 3 3.5-3.5L21 17"/></svg>',
    "clock": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" aria-hidden="true"><circle cx="10" cy="10" r="7.5"/><path d="M10 5.8V10l2.8 2"/></svg>',
    "target": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" aria-hidden="true"><circle cx="10" cy="10" r="7.5"/><circle cx="10" cy="10" r="3.6"/><circle cx="10" cy="10" r=".8" fill="currentColor"/></svg>',
    "arrow-right": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 10h11M10.5 5.5L15 10l-4.5 4.5"/></svg>',
    # Section / card icons
    "home": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 8.5L10 3l7 5.5V16a1 1 0 01-1 1h-3.5v-5h-5v5H4a1 1 0 01-1-1z"/></svg>',
    "rocket": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M11 3c3.5 0 6 2.5 6 6 0 3-3 6.5-6 8-3-1.5-6-5-6-8 0-3.5 2.5-6 6-6z"/><circle cx="11" cy="8" r="1.8"/><path d="M7.5 14.5L5 17l2.5-.5"/></svg>',
    "window": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round" aria-hidden="true"><rect x="2.5" y="3.5" width="15" height="13" rx="2"/><path d="M2.5 7.5h15M6.5 3.5v4"/></svg>',
    "edit": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M13.2 3.3l3.5 3.5L7.5 16H4v-3.5z"/><path d="M11.5 5l3.5 3.5"/></svg>',
    "find": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" aria-hidden="true"><circle cx="8.5" cy="8.5" r="5.5"/><path d="M12.7 12.7L17 17"/></svg>',
    "compass": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round" aria-hidden="true"><circle cx="10" cy="10" r="7.5"/><path d="M13.2 6.8l-1.7 4.7-4.7 1.7 1.7-4.7z"/></svg>',
    "bolt": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round" aria-hidden="true"><path d="M11 2.5L4.5 11H9l-.5 6.5L15.5 9H11z"/></svg>',
    "folder": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round" aria-hidden="true"><path d="M2.5 5.5A1.5 1.5 0 014 4h3.6l1.5 2H16a1.5 1.5 0 011.5 1.5v7A1.5 1.5 0 0116 16H4a1.5 1.5 0 01-1.5-1.5z"/></svg>',
    "build": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12.8 2.8a4 4 0 00-5 5L3 12.6V17h4.4l4.8-4.8a4 4 0 005-5L14.5 9.5 11 9l-.5-3.5z"/></svg>',
    "bug": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" aria-hidden="true"><rect x="6" y="6.5" width="8" height="9" rx="4"/><path d="M6 10H2.5M17.5 10H14M6.5 13.5l-3 2M13.5 13.5l3 2M6.5 7l-3-2M13.5 7l3-2M8 5.5a2 2 0 014 0"/></svg>',
    "sliders": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" aria-hidden="true"><path d="M3 6h9M15 6h2M3 14h3M9 14h8"/><circle cx="13" cy="6" r="2"/><circle cx="7" cy="14" r="2"/></svg>',
    "keyboard": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="1.5" y="5" width="17" height="10" rx="2"/><path d="M5 8h.01M8 8h.01M11 8h.01M14 8h.01M5 11h.01M15 11h.01M7.5 11h5"/></svg>',
    "book": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 4.5A1.5 1.5 0 014.5 3H9v14H4.5A1.5 1.5 0 013 15.5z"/><path d="M17 4.5A1.5 1.5 0 0015.5 3H11v14h4.5a1.5 1.5 0 001.5-1.5z"/></svg>',
    "help": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" aria-hidden="true"><circle cx="10" cy="10" r="7.5"/><path d="M8 7.8a2.1 2.1 0 113 1.9c-.6.4-1 .8-1 1.6M10 14.4v.1"/></svg>',
    "list": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" aria-hidden="true"><path d="M7 5.5h10M7 10h10M7 14.5h10M3.5 5.5h.01M3.5 10h.01M3.5 14.5h.01"/></svg>',
    "graduation": '<svg viewBox="0 0 20 20" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M10 3l8 4-8 4-8-4z"/><path d="M5 9v4.2c0 1.3 2.2 2.3 5 2.3s5-1 5-2.3V9"/></svg>',
}


def icon(name, cls=None):
    svg = ICONS.get(name, "")
    if cls and svg:
        svg = svg.replace("<svg ", '<svg class="%s" ' % cls, 1)
    return svg


# --------------------------------------------------------------------------
# Content authoring helpers  (used by the content_*.py modules)
# --------------------------------------------------------------------------

def esc(text):
    return html.escape(text, quote=False)


def slugify(text):
    s = re.sub(r"<[^>]+>", "", text)
    s = s.lower().strip()
    s = re.sub(r"[^a-z0-9\s-]", "", s)
    s = re.sub(r"[\s-]+", "-", s)
    return s.strip("-")


def h2(text, anchor=None):
    return '<h2 id="%s">%s</h2>' % (anchor or slugify(text), text)


def h3(text, anchor=None):
    return '<h3 id="%s">%s</h3>' % (anchor or slugify(text), text)


def h4(text):
    return "<h4>%s</h4>" % text


def p(text):
    return "<p>%s</p>" % text


def ul(items):
    return "<ul>\n%s\n</ul>" % "\n".join("  <li>%s</li>" % i for i in items)


def ol(items, steps=False):
    cls = ' class="steps"' if steps else ""
    return "<ol%s>\n%s\n</ol>" % (cls, "\n".join("  <li>%s</li>" % i for i in items))


def dl(pairs):
    rows = "\n".join(
        '  <div class="row"><dt>%s</dt><dd>%s</dd></div>' % (k, v) for k, v in pairs
    )
    return '<dl class="deflist">\n%s\n</dl>' % rows


def callout(kind, title, body):
    """kind: note | tip | warning | important | todo"""
    ico = icon(kind if kind in ICONS else "note")
    return (
        '<div class="callout %s">%s<div class="callout-body">'
        '<span class="callout-title">%s</span>%s</div></div>'
        % (kind, ico, esc(title), body if body.startswith("<") else "<p>%s</p>" % body)
    )


def note(body, title="Note"):
    return callout("note", title, body)


def tip(body, title="Tip"):
    return callout("tip", title, body)


def warn(body, title="Warning"):
    return callout("warning", title, body)


def important(body, title="Important"):
    return callout("important", title, body)


def todo(body, title="TODO"):
    return callout("todo", title, body)


def code(source, lang="fb", title=None, numbered=True):
    cls = "code-block numbered" if numbered else "code-block"
    head = (
        '<div class="code-head">'
        '<span class="code-lang">%s</span>'
        '<span class="code-title">%s</span>'
        '<button class="copy-btn" type="button">%s<span class="copy-label">Copy</span></button>'
        "</div>" % (esc(lang), esc(title or ""), icon("copy"))
    )
    return '<div class="%s" data-lang="%s">%s<pre><code>%s</code></pre></div>' % (
        cls,
        esc(lang),
        head,
        esc(source.strip("\n")),
    )


def table(headers, rows, key_first=False):
    thead = "".join(
        '<th%s>%s</th>' % (' class="col-key"' if key_first and i == 0 else "", hcell)
        for i, hcell in enumerate(headers)
    )
    body = []
    for r in rows:
        cells = "".join(
            "<td%s>%s</td>" % (' class="col-key"' if key_first and i == 0 else "", c)
            for i, c in enumerate(r)
        )
        body.append("<tr>%s</tr>" % cells)
    return (
        '<div class="table-wrap"><table><thead><tr>%s</tr></thead><tbody>%s</tbody></table></div>'
        % (thead, "".join(body))
    )


def figure_img(src, caption, alt=None):
    return (
        '<figure><img src="%s" alt="%s" loading="lazy">'
        "<figcaption>%s</figcaption></figure>"
        % (src, esc(alt or re.sub(r"<[^>]+>", "", caption)), caption)
    )


def placeholder(title, hint, caption=None):
    """A clearly-marked stand-in for a screenshot that has not been taken yet."""
    fig = (
        '<div class="placeholder-fig">%s<div><span class="ph-title">%s</span>'
        '<span class="ph-hint">%s</span></div></div>'
        % (icon("image"), esc(title), esc(hint))
    )
    cap = (
        "<figcaption><strong>Placeholder.</strong> %s</figcaption>" % caption
        if caption
        else ""
    )
    return "<figure>%s%s</figure>" % (fig, cap)


def diagram(svg_body, caption, viewbox="0 0 800 380"):
    """An annotated SVG diagram. Colours come from CSS variables so it themes."""
    return (
        '<figure><div class="fig-frame">'
        '<svg viewBox="%s" xmlns="http://www.w3.org/2000/svg" role="img">%s</svg>'
        "</div><figcaption>%s</figcaption></figure>" % (viewbox, svg_body, caption)
    )


def cards(items, base=""):
    """items: list of (href, icon_name, title, description)"""
    out = []
    for href, ico, title, desc in items:
        out.append(
            '<a class="card" href="%s%s">%s<h3>%s</h3><p>%s</p></a>'
            % (base, href, icon(ico, "card-icon"), esc(title), esc(desc))
        )
    return '<div class="card-grid">%s</div>' % "".join(out)


def faq(items):
    """items: list of (question, answer_html)"""
    out = []
    for q, a in items:
        out.append(
            '<details class="faq-item"><summary>%s</summary>'
            '<div class="faq-body">%s</div></details>' % (esc(q), a)
        )
    return "".join(out)


def kbd(*keys):
    return " + ".join("<kbd>%s</kbd>" % esc(k) for k in keys)


def menu(*parts):
    sep = '<span class="sep">›</span>'
    return '<span class="ui path">%s</span>' % sep.join(esc(x) for x in parts)


def ui(text):
    return '<span class="ui">%s</span>' % esc(text)


def lesson_meta(goal, time_est, prereq):
    return (
        '<div class="lesson-meta">'
        '<span class="lm-item">%s<span><strong>Goal:</strong> %s</span></span>'
        '<span class="lm-item">%s<span><strong>Time:</strong> %s</span></span>'
        '<span class="lm-item">%s<span><strong>Before you start:</strong> %s</span></span>'
        "</div>" % (icon("target"), goal, icon("clock"), time_est, icon("book"), prereq)
    )


# --------------------------------------------------------------------------
# Page registry
# --------------------------------------------------------------------------

SECTIONS = []          # ordered [{id, title, icon, pages: [slug, ...]}]
PAGES = {}             # slug -> page dict
_ORDER = []            # flat reading order of slugs


def section(sid, title, icon_name):
    sec = {"id": sid, "title": title, "icon": icon_name, "pages": []}
    SECTIONS.append(sec)
    return sec


def page(slug, title, section_id, summary, body, keywords=""):
    PAGES[slug] = {
        "slug": slug,
        "title": title,
        "section": section_id,
        "summary": summary,
        "body": body,
        "keywords": keywords,
    }
    for sec in SECTIONS:
        if sec["id"] == section_id:
            sec["pages"].append(slug)
            break
    else:
        raise ValueError("Unknown section %r for page %r" % (section_id, slug))
    _ORDER.append(slug)


# --------------------------------------------------------------------------
# Rendering
# --------------------------------------------------------------------------

def build_nav(current_slug):
    out = ['<nav class="nav-tree" aria-label="Documentation sections">']
    out.append(
        '<ul>'
    )
    for sec in SECTIONS:
        has_current = current_slug in sec["pages"]
        expanded = "true" if has_current else "false"
        out.append('<li class="nav-section">')
        out.append(
            '<button class="nav-section-btn" type="button" data-section="%s" '
            'aria-expanded="%s" aria-controls="navsec-%s">%s%s<span>%s</span></button>'
            % (sec["id"], expanded, sec["id"], icon("chevron"),
               icon(sec["icon"], "sec-icon"), esc(sec["title"]))
        )
        out.append('<div class="nav-section-body" id="navsec-%s"><ul>' % sec["id"])
        for slug in sec["pages"]:
            pg = PAGES[slug]
            cur = ' aria-current="page"' if slug == current_slug else ""
            out.append('<li><a href="%s.html"%s>%s</a></li>' % (slug, cur, esc(pg["title"])))
        out.append("</ul></div></li>")
    out.append("</ul></nav>")
    return "\n".join(out)


def build_breadcrumbs(pg):
    sec = next((s for s in SECTIONS if s["id"] == pg["section"]), None)
    crumbs = ['<li><a href="index.html">Home</a></li>']
    if sec:
        first = sec["pages"][0]
        crumbs.append('<li><a href="%s.html">%s</a></li>' % (first, esc(sec["title"])))
    crumbs.append('<li><span aria-current="page">%s</span></li>' % esc(pg["title"]))
    return (
        '<nav class="breadcrumbs" aria-label="Breadcrumb"><ol>%s</ol></nav>'
        % "".join(crumbs)
    )


def build_pager(slug):
    idx = _ORDER.index(slug)
    prev_slug = _ORDER[idx - 1] if idx > 0 else None
    next_slug = _ORDER[idx + 1] if idx < len(_ORDER) - 1 else None
    if not prev_slug and not next_slug:
        return ""
    parts = ['<nav class="pager" aria-label="Page navigation">']
    if prev_slug:
        parts.append(
            '<a class="prev" href="%s.html" rel="prev">'
            '<span class="dir">← Previous</span>'
            '<span class="ttl">%s</span></a>' % (prev_slug, esc(PAGES[prev_slug]["title"]))
        )
    if next_slug:
        parts.append(
            '<a class="next" href="%s.html" rel="next">'
            '<span class="dir">Next →</span>'
            '<span class="ttl">%s</span></a>' % (next_slug, esc(PAGES[next_slug]["title"]))
        )
    parts.append("</nav>")
    return "".join(parts)


HEAD_SCRIPT = """(function(){try{var t=localStorage.getItem('tiko-help-theme');
if(!t)t=window.matchMedia&&window.matchMedia('(prefers-color-scheme: dark)').matches?'dark':'light';
document.documentElement.setAttribute('data-theme',t);}catch(e){}})();"""


SHELL = """<!DOCTYPE html>
<html lang="en" data-theme="light" data-base="">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="{description}">
<meta name="generator" content="Tiko Editor Help build.py">
<title>{title} — {site}</title>
<link rel="stylesheet" href="assets/styles.css">
<link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><rect width='32' height='32' rx='7' fill='%231f6feb'/><text x='16' y='22' font-family='monospace' font-size='16' font-weight='bold' fill='white' text-anchor='middle'>tk</text></svg>">
<script>{headscript}</script>
</head>
<body>
<a class="skip-link" href="#content">Skip to main content</a>

<header class="site-header">
  <button class="icon-btn" id="nav-toggle" type="button" aria-label="Toggle navigation" aria-expanded="false" aria-controls="sidebar">{menuicon}</button>
  <a class="brand" href="index.html">
    <span class="brand-mark" aria-hidden="true">tk</span>
    <span>Tiko Editor</span>
    <span class="brand-sub">{tagline}</span>
  </a>
  <span class="header-spacer"></span>
  <button class="search-launcher" id="search-launcher" type="button" aria-label="Search documentation">
    {searchicon}<span class="sl-label">Search docs</span><kbd>Ctrl K</kbd>
  </button>
  <button class="icon-btn" id="theme-toggle" type="button" aria-label="Switch theme" title="Switch theme">{sunicon}{moonicon}</button>
</header>

<div class="nav-scrim" id="nav-scrim"></div>

<div class="layout">
  <aside class="sidebar" id="sidebar">
{nav}
  </aside>

  <div class="main-col">
    <main class="content" id="content">
{breadcrumbs}
      <h1>{h1}</h1>
{summary}
{body}
{pager}
      <footer class="site-footer">
        <span>{versionlabel}</span>
        <span>·</span>
        <a href="index.html">Home</a>
        <a href="doc-index.html">Index</a>
        <a href="glossary.html">Glossary</a>
        <a href="faq.html">FAQ</a>
      </footer>
    </main>

    <aside class="toc" id="page-toc" aria-label="On this page"></aside>
  </div>
</div>

<button class="back-to-top" id="back-to-top" type="button" aria-label="Back to top" title="Back to top">{topicon}</button>

<div class="search-overlay" id="search-overlay" role="dialog" aria-modal="true" aria-label="Search documentation">
  <div class="search-panel">
    <div class="search-inputrow">
      {searchicon}
      <input type="search" id="search-input" placeholder="Search the documentation…" autocomplete="off" spellcheck="false" aria-label="Search query">
      <button class="icon-btn" id="search-close" type="button" aria-label="Close search">{closeicon}</button>
    </div>
    <ul class="search-results" id="search-results"></ul>
    <div class="search-foot">
      <span><kbd>↑</kbd><kbd>↓</kbd> navigate</span>
      <span><kbd>Enter</kbd> open</span>
      <span><kbd>Esc</kbd> close</span>
    </div>
  </div>
</div>

<script src="assets/search-index.js"></script>
<script src="assets/script.js"></script>
</body>
</html>
"""


def render_page(slug):
    pg = PAGES[slug]
    summary_html = (
        '<p class="page-summary">%s</p>' % pg["summary"] if pg["summary"] else ""
    )
    description = re.sub(r"<[^>]+>", "", pg["summary"] or pg["title"])[:180]

    return SHELL.format(
        title=esc(pg["title"]),
        site=esc(SITE_TITLE),
        tagline=esc(SITE_TAGLINE),
        description=esc(description),
        headscript=HEAD_SCRIPT,
        nav=build_nav(slug),
        breadcrumbs="" if slug == "index" else build_breadcrumbs(pg),
        h1=esc(pg["title"]),
        summary=summary_html,
        body=pg["body"],
        pager=build_pager(slug),
        versionlabel=esc(VERSION_LABEL),
        menuicon=icon("menu"),
        searchicon=icon("search"),
        sunicon=icon("sun"),
        moonicon=icon("moon"),
        topicon=icon("top"),
        closeicon=icon("close"),
    )


TAG_RE = re.compile(r"<[^>]+>")
WS_RE = re.compile(r"\s+")


def plain_text(html_src):
    text = re.sub(r"<(script|style)[^>]*>.*?</\1>", " ", html_src, flags=re.S | re.I)
    text = TAG_RE.sub(" ", text)
    text = html.unescape(text)
    return WS_RE.sub(" ", text).strip()


def build_search_index():
    entries = []
    for slug in _ORDER:
        pg = PAGES[slug]
        sec = next((s for s in SECTIONS if s["id"] == pg["section"]), None)
        body = plain_text(pg["summary"] + " " + pg["body"])
        entries.append(
            {
                "u": slug + ".html",
                "t": pg["title"],
                "s": sec["title"] if sec else "",
                "k": pg["keywords"],
                "b": body[:2400],
            }
        )
    payload = json.dumps(entries, ensure_ascii=False, separators=(",", ":"))
    return (
        "/* Generated by tools/build.py — do not edit by hand. */\n"
        "window.TIKO_SEARCH_INDEX = %s;\n" % payload
    )


def write(path, text):
    full = os.path.join(OUT, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)


def main():
    sys.path.insert(0, HERE)

    # Content modules register their sections and pages on import, in order.
    import content_welcome      # noqa: F401
    import content_ui           # noqa: F401
    import content_editing      # noqa: F401
    import content_search_nav   # noqa: F401
    import content_projects     # noqa: F401
    import content_build_debug  # noqa: F401
    import content_customize    # noqa: F401
    import content_reference    # noqa: F401
    import content_reference2   # noqa: F401
    import content_tutorial     # noqa: F401
    import content_appendix     # noqa: F401

    count = 0
    for slug in _ORDER:
        write(slug + ".html", render_page(slug))
        count += 1

    write("assets/search-index.js", build_search_index())

    print("Tiko Editor help built: %d pages, %d sections" % (count, len(SECTIONS)))
    for sec in SECTIONS:
        print("  %-22s %d pages" % (sec["title"], len(sec["pages"])))
    print("Output: %s" % OUT)


if __name__ == "__main__":
    # Re-enter through the imported module so that the content modules — which do
    # "from build import ..." — share this module's registries rather than getting a
    # second, empty copy of them under a different module name.
    sys.path.insert(0, HERE)
    import build as _build
    _build.main()
