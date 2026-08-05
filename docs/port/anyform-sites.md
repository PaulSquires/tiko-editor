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
