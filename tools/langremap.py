"""
langremap.py -- compact tiko's localization ids.

Keeps every id reached by a known call site, drops the blanks and the dead strings, and
renumbers what is left contiguously from 0 PRESERVING ORDER (so an id range in a self-test
stays a range). Rewrites the six .lang files and every call site in src.

Verify with langverify.py compare against a snapshot taken BEFORE running this. That check
is the point: it compares the English TEXT each site resolves to, which is the only thing
that catches a site left un-remapped - it still resolves, just to the wrong string.

usage: python langremap.py <tiko-root>
"""
import os, re, sys, glob

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import langverify as lv


def build_mapping(root):
    en = lv.read_english(root)
    sites = lv.extract(root)
    keep = set()
    for s in sites:
        if s['form'] == 'range':
            keep.update(range(s['id'], s['id_hi'] + 1))
        else:
            keep.add(s['id'])
    keep = sorted(i for i in keep if i in en)          # ignore ids with no entry at all
    mapping = {old: new for new, old in enumerate(keep)}

    # a range must stay contiguous, or its loop bounds no longer describe it
    for s in sites:
        if s['form'] != 'range':
            continue
        span = list(range(s['id'], s['id_hi'] + 1))
        news = [mapping[i] for i in span]
        assert news == list(range(news[0], news[0] + len(news))), \
            'range %d..%d in %s:%d would stop being contiguous' % (
                s['id'], s['id_hi'], s['file'], s['line'])
    return en, sites, mapping


def rewrite_lang(root, mapping):
    for path in sorted(glob.glob(os.path.join(root, 'settings', 'languages', '*.lang'))):
        raw = open(path, 'rb').read()
        assert raw[:2] == b'\xff\xfe'
        out, ids = [], {}
        for line in raw[2:].decode('utf-16-le').split('\r\n'):
            m = re.match(r'^(\d{5}):(.*)$', line)
            if m:
                ids[int(m.group(1))] = m.group(2)
            elif line.startswith('MAXIMUM:'):
                out.append('@@MAXIMUM@@')
            else:
                out.append(line)                      # header comments, blank lines
        body = ['%05d:%s' % (mapping[o], ids.get(o, '')) for o in sorted(mapping)]
        last = max(mapping.values())
        final = []
        for l in out:
            if l == '@@MAXIMUM@@':
                final.append('MAXIMUM:%d' % last)
                final.extend(body)
            else:
                final.append(l)
        open(path, 'wb').write(b'\xff\xfe' + '\r\n'.join(final).encode('utf-16-le'))
        print('  %-26s %d ids, MAXIMUM:%d' % (os.path.basename(path), len(body), last))


def rewrite_sources(root, mapping):
    """Rewrite ids in place. Operates on whole-file text so a `_` continuation inside a
    positional call is handled; every substitution is anchored on a form we know."""
    changed = 0
    paths = []
    for pat in ('*.bas', '*.bi', '*.inc'):
        paths += glob.glob(os.path.join(root, 'src', pat))

    call_names = '|'.join(lv.POSITIONAL)
    for path in sorted(paths):
        name = os.path.basename(path)
        if name in lv.SKIP_FILES:
            continue
        text = open(path, encoding='latin-1').read()
        orig = text

        def sub_L(m):
            old = int(m.group(2))
            return '%s(%d%s' % (m.group(1), mapping[old], m.group(3)) if old in mapping else m.group(0)
        text = re.sub(r'\b(LL?)\(\s*(\d+)\s*([,)])', sub_L, text)

        # positional: id is the Nth argument, and the call may span lines via `_`
        def sub_call(m):
            fn, args = m.group(1), m.group(2)
            argpos = lv.POSITIONAL[fn]
            parts = args.split(',')
            if argpos >= len(parts):
                return m.group(0)
            tok = parts[argpos]
            mm = re.fullmatch(r'(\s*)(\d+)(\s*)', tok)
            if not mm or int(mm.group(2)) not in mapping:
                return m.group(0)
            parts[argpos] = '%s%d%s' % (mm.group(1), mapping[int(mm.group(2))], mm.group(3))
            return '%s(%s)' % (fn, ','.join(parts))
        text = re.sub(r'\b(' + call_names + r')\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)',
                      sub_call, text, flags=re.S)

        def sub_field(m):
            old = int(m.group(2))
            return '%s = %d' % (m.group(1), mapping[old]) if old in mapping else m.group(0)
        text = re.sub(r'\b(nLabelID|nCaptionID)\s*=\s*(\d+)\b', sub_field, text)

        open(path, 'w', encoding='latin-1').write(text)
        if text != orig:
            changed += 1
    print('  rewrote ids in %d source files' % changed)


def rewrite_ranges(root, mapping, sites):
    """Range bounds are rewritten separately and by LINE, because `for i as long = 370 to
    385` is syntactically identical to any other numeric FOR - only the site list knows
    which ones index LL()."""
    byfile = {}
    for s in sites:
        if s['form'] == 'range':
            byfile.setdefault(s['file'], []).append(s)
    for name, ss in byfile.items():
        path = os.path.join(root, 'src', name)
        lines = open(path, encoding='latin-1').read().split('\n')
        for s in ss:
            i = s['line'] - 1
            lo, hi = mapping[s['id']], mapping[s['id_hi']]
            new = re.sub(r'(\bfor\s+\w+\s+as\s+\w+\s*=\s*)\d+(\s+to\s+)\d+\b',
                         lambda m: '%s%d%s%d' % (m.group(1), lo, m.group(2), hi),
                         lines[i], count=1)
            lines[i] = new
        open(path, 'w', encoding='latin-1').write('\n'.join(lines))
        print('  %-24s %d range(s) rebased' % (name, len(ss)))


if __name__ == '__main__':
    root = sys.argv[1]
    en, sites, mapping = build_mapping(root)
    print('keeping %d ids, dropping %d (%d dead strings + %d blanks)' % (
        len(mapping),
        len(en) - len(mapping),
        len([i for i, v in en.items() if v.strip() and i not in mapping]),
        len([i for i, v in en.items() if not v.strip()])))
    # RANGES FIRST: they are rewritten by line number, and the other rewrites do not move
    # lines, but doing them first keeps the site list's line numbers unambiguous.
    rewrite_ranges(root, mapping, sites)
    rewrite_sources(root, mapping)
    rewrite_lang(root, mapping)
