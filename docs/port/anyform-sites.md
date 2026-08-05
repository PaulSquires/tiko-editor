# The four `Any`-form sites

fbc's `Trim(s, Any "chars")` trims any character from a SET. `Any` is syntax,
not an argument, so there is nothing a forwarding function can be handed --
`PsTrim(s, Any " +")` does not parse. These four were therefore left as fbc
intrinsics by the 557-site conversion and will break loudly when `DWSTRING`
becomes PsCore's type.

    src/modEncoding.inc:166    RTrim( wszOut, any !" \r\n" )
    src/clsDocument.inc:717    RTrim(buffer, any chr(13,10))
    src/clsDocument.inc:929    Trim( st, any "( )" )
    src/frmUserTools.inc:230   Trim(... , any " +")

Breaking loudly is the good case and is why they are listed rather than
worked around: each needs a real decision, not a mechanical substitution.

PsStr.bi has no set-based trim today. Adding `PsTrimAny(s, sSet)` alongside
the existing `PsStrRemoveAny` would cover all four, and matches the naming the
"Any" family already uses -- see the note in PsStr.bi on why "Any" means a set
of single characters rather than a substring.

# Sites where the conversion changed the CALL, not just the name

Everything else in Phase 7a is a forwarder, so the self-tests can attribute any
movement to a mistake. These four are the exceptions and are listed so they are
not lost among 1065 mechanical substitutions.

## `PsFileCopy`, clsConfig.inc:56

Was `AfxCopyFile(a.sptr, b.sptr, true)` -- AfxCopyFile takes LPCWSTR, so the
call passed pointers. `PsFileCopy` takes the strings, so the two `.sptr` came
off. No semantic change; the pointers were the same strings.

## `PsPathRelativeTo`, clsConfig.inc:1087 and :1121

Was Win32's four-argument shape, through AfxNova:

    AfxPathRelativePathTo(gApp.ProjectFilename, FILE_ATTRIBUTE_NORMAL,
                          wszText, FILE_ATTRIBUTE_NORMAL)

`from` was the project FILE with a file attribute, which PathRelativePathTo
resolves against that file's DIRECTORY. PsPathRelativeTo takes a base
DIRECTORY, so the call now passes it explicitly:

    PsPathRelativeTo(wszText, PsPathDirWithSep(gApp.ProjectFilename))

Believed equivalent -- same directory either way -- but it is a REASONED
equivalence rather than a measured one. The 27 stable self-test suites are
unchanged across three runs, which is evidence and not proof: no suite is known
to exercise project-relative path storage specifically. Worth an interactive
check that a project saved with relative paths still reloads.
