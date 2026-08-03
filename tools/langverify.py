"""
langverify.py -- value-based verification for tiko's localization ids.

WHY THIS EXISTS. Every localization guard in tiko is `len(LL(id)) > 0`, which detects a
MISSING string and never a WRONG one. After a renumbering, a call site whose id was not
remapped still resolves - to some other real string - and the whole assertion suite stays
green while the UI shows plausible but wrong text. So a renumber has to be verified by
VALUE: the English text each call site resolves to must be identical before and after.

  snapshot  -- write (file, line, form, id, english-text) for every id-bearing site
  compare   -- re-extract and diff against a snapshot, by TEXT, keyed on (file, line, form)
  coverage  -- how many of english.lang's non-blank strings are reached by a known site

COVERAGE IS THE COMPLETENESS TEST. The danger is not a site that is remapped wrongly - the
comparison catches that. It is a site this extractor does not KNOW about, which would be
neither remapped nor verified. If every non-blank string in english.lang is accounted for by
at least one site, the extractor has found every form that matters.

usage:  python langverify.py snapshot <tiko-root> <out.json>
        python langverify.py compare  <tiko-root> <out.json>
        python langverify.py coverage <tiko-root>
"""
import json, os, re, sys, glob

# --- the five syntactic forms an id can take ------------------------------------------
# 1. L(nnn, "...")  /  LL(nnn)
RE_L = re.compile(r'\b(LL?)\(\s*(\d+)\s*[,)]')
# 2/3. bare positional argument - id is the Nth arg of a known signature
POSITIONAL = {
    'OptionsRows_Add':        2,   # ( page, kind, ID, @field, ... )
    'frmFormatOptions_AddRow': 2,  # ( page, kind, ID, @field, ... )
    'MsgBox_Word':            0,   # ( ID, "fallback" )
}
RE_CALL = re.compile(r'\b(' + '|'.join(POSITIONAL) + r')\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)')
# 4. a row-table field assigned a literal
RE_FIELD = re.compile(r'\b(nLabelID|nCaptionID)\s*=\s*(\d+)\b')
# 5. an id RANGE in a self-test: for x as long = N to M
# A numeric FOR is only an ID RANGE if its body actually indexes LL() with the loop
# variable. Without that test the heuristic swept up `for i = 65 to 90` (ASCII 'A'..'Z' in
# modKeyBindings), `48 to 57` ('0'..'9') and item loops like `1 to 99` - remapping those
# would have corrupted working code, and they were also inflating coverage by spuriously
# reaching every id they spanned.
RE_RANGE = re.compile(r'\bfor\s+(\w+)\s+as\s+\w+\s*=\s*(\d+)\s+to\s+(\d+)\b')

SKIP_FILES = {'PsColorPicker.bi'}          # documentation mentioning L(e,s)


def read_english(root):
    p = os.path.join(root, 'settings', 'languages', 'english.lang')
    raw = open(p, 'rb').read()
    assert raw[:2] == b'\xff\xfe', 'english.lang is not UTF-16LE with a BOM'
    ids = {}
    for line in raw[2:].decode('utf-16-le').split('\r\n'):
        m = re.match(r'^(\d{5}):(.*)$', line)
        if m:
            ids[int(m.group(1))] = m.group(2)
    return ids


def strip_comment(line):
    """Drop a trailing FB comment, respecting string literals."""
    out, in_str = [], False
    i = 0
    while i < len(line):
        c = line[i]
        if c == '"':
            in_str = not in_str
        elif not in_str and (c == "'" or line[i:i+4].upper() == ' REM '):
            break
        out.append(c)
        i += 1
    return ''.join(out)


def extract(root):
    """-> list of sites: dict(file, line, form, id)"""
    sites = []
    srcdir = os.path.join(root, 'src')
    paths = []
    for pat in ('*.bas', '*.bi', '*.inc'):        # .inc is where most of tiko lives
        paths += glob.glob(os.path.join(srcdir, pat))
    for path in sorted(paths):
        name = os.path.basename(path)
        if name in SKIP_FILES:
            continue
        # FB continues a statement with a trailing '_', and OptionsRows_Add calls do
        # exactly that - so the closing paren often sits on the NEXT physical line.
        # Matching per physical line silently missed those, which is how a renumber
        # would have broken them. Fold continuations into one logical line, keyed on
        # the line the statement STARTS on.
        logical = []
        buf, startline = '', None
        for n, raw in enumerate(open(path, encoding='latin-1'), 1):
            piece = strip_comment(raw).rstrip()
            if startline is None:
                startline = n
            if piece.endswith('_'):
                buf += piece[:-1] + ' '
                continue
            logical.append((startline, buf + piece))
            buf, startline = '', None
        if buf:
            logical.append((startline or 1, buf))

        for idx, (n, line) in enumerate(logical):
            for m in RE_L.finditer(line):
                sites.append(dict(file=name, line=n, form='L', id=int(m.group(2))))
            for m in RE_CALL.finditer(line):
                fn, args = m.group(1), m.group(2)
                parts = [a.strip() for a in args.split(',')]
                # NOT 'idx' - that is the enclosing loop's line index, and shadowing it
                # sent the range look-ahead below to the wrong lines, silently finding no
                # id ranges at all.
                argpos = POSITIONAL[fn]
                if argpos < len(parts) and re.fullmatch(r'\d+', parts[argpos]):
                    sites.append(dict(file=name, line=n, form=fn, id=int(parts[argpos])))
            for m in RE_FIELD.finditer(line):
                sites.append(dict(file=name, line=n, form='field', id=int(m.group(2))))
            for m in RE_RANGE.finditer(line):
                var, lo, hi = m.group(1), int(m.group(2)), int(m.group(3))
                if not (0 < hi - lo < 200 and lo >= 0):
                    continue
                nxt = ' '.join(t for _, t in logical[idx + 1: idx + 12])
                if re.search(r'\bLL?\(\s*' + re.escape(var) + r'\s*[,)]', nxt):
                    sites.append(dict(file=name, line=n, form='range', id=lo, id_hi=hi))


    seen = {}
    for st in sites:
        k = (st['file'], st['line'], st['form'])
        st['ord'] = seen.get(k, 0)
        seen[k] = st['ord'] + 1
    return sites


def key(s):
    # The ORDINAL matters: two L() calls can share one logical line (`L(0,"OK")` and
    # `L(1,"Cancel")` on the same AddButton line). Keying on file:line:form alone collapsed
    # 94 of 869 sites into 775, and a wrong remap of the second one would have been
    # invisible - the checker would have compared the first against itself and passed.
    return '%s:%d:%s:%d' % (s['file'], s['line'], s['form'], s['ord'])


def resolve(site, en):
    if site['form'] == 'range':
        return ' | '.join(en.get(i, '<absent>') for i in range(site['id'], site['id_hi'] + 1))
    return en.get(site['id'], '<absent>')


def cmd_snapshot(root, out):
    en = read_english(root)
    sites = extract(root)
    data = [dict(k=key(s), id=s['id'], hi=s.get('id_hi'), text=resolve(s, en)) for s in sites]
    json.dump(data, open(out, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print('snapshot: %d sites -> %s' % (len(data), out))


def cmd_compare(root, snap):
    en = read_english(root)
    before = {d['k']: d for d in json.load(open(snap, encoding='utf-8'))}
    after = {key(s): dict(id=s['id'], hi=s.get('id_hi'), text=resolve(s, en))
             for s in extract(root)}

    bad = 0
    for k, b in sorted(before.items()):
        a = after.get(k)
        if a is None:
            print('  LOST   %-46s (was id %s)' % (k, b['id'])); bad += 1
        elif a['text'] != b['text']:
            print('  CHANGED %-45s id %s->%s' % (k, b['id'], a['id']))
            print('           was: %r' % b['text'][:70])
            print('           now: %r' % a['text'][:70]); bad += 1
    for k in sorted(set(after) - set(before)):
        print('  NEW    %-46s id %s' % (k, after[k]['id'])); bad += 1

    print('compare: %d sites, %d differing' % (len(before), bad))
    return 1 if bad else 0


def cmd_coverage(root):
    en = read_english(root)
    nonblank = set(k for k, v in en.items() if v.strip() != '')
    seen = set()
    byform = {}
    for s in extract(root):
        byform[s['form']] = byform.get(s['form'], 0) + 1
        if s['form'] == 'range':
            seen.update(range(s['id'], s['id_hi'] + 1))
        else:
            seen.add(s['id'])

    print('sites by form:')
    for f, c in sorted(byform.items(), key=lambda x: -x[1]):
        print('   %-24s %d' % (f, c))
    print()
    print('english.lang non-blank : %d' % len(nonblank))
    print('reached by a known site: %d' % len(nonblank & seen))
    missing = sorted(nonblank - seen)
    print('NOT reached            : %d' % len(missing))
    if missing:
        print()
        print('  Each of these is either a form the extractor does not know (which a')
        print('  renumber would silently break) or a genuinely dead string:')
        for i in missing:
            print('    %3d  %r' % (i, en[i][:64]))
    dangling = sorted(i for i in seen if i not in en)
    if dangling:
        print('\n  referenced but ABSENT from english.lang: %s' % dangling)
    return 1 if missing else 0


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(2)
    cmd, root = sys.argv[1], sys.argv[2]
    if cmd == 'snapshot':
        sys.exit(cmd_snapshot(root, sys.argv[3]) or 0)
    if cmd == 'compare':
        sys.exit(cmd_compare(root, sys.argv[3]))
    if cmd == 'coverage':
        sys.exit(cmd_coverage(root))
    print(__doc__); sys.exit(2)
