' FreeBasic port of fast fuzzy finder written in C.
' https://github.com/jhawthorn/fzy

'#include once "crt/limits.bi"    '' provides DBL_MAX

#define SCORE_GAP_LEADING       -0.005
#define SCORE_GAP_TRAILING      -0.005
#define SCORE_GAP_INNER         -0.01
#define SCORE_MATCH_CONSECUTIVE  1.0
#define SCORE_MATCH_SLASH        0.9
#define SCORE_MATCH_WORD         0.8
#define SCORE_MATCH_CAPITAL      0.7
#define SCORE_MATCH_DOT          0.6

type score_t as double

type fuzzy_match_struct
	needle_len       as integer
	haystack_len     as integer
	lower_needle     as DWSTRING
	lower_haystack   as DWSTRING
	match_bonus(any) as score_t   '' dynamic array, redim'd in fuzzy_precompute_bonus()
end type


'#define SCORE_MAX DBL_MAX
'#define SCORE_MIN (-DBL_MAX)
#define SCORE_MAX 9999999999
#define SCORE_MIN (-SCORE_MAX)

declare function fuzzy_has_match( byval needle as DWSTRING, byval haystack as DWSTRING ) as boolean
declare function fuzzy_match_postions( byval needle as DWSTRING, byval haystack as DWSTRING, positions() as uinteger ) as score_t
declare function fuzzy_match( byval needle as DWSTRING, byval haystack as DWSTRING ) as score_t

'' ---------------------------------------------------------------------
'' Call fuzzy_init_bonus_tables() once, before bonus_states()/bonus_index()
'' are used (e.g. at the start of your program).
''
'' These lookup tables are indexed by raw byte value (0-255), so a
'' fixed-size 256-element array is the natural, correctly-sized
'' representation here.
'' ---------------------------------------------------------------------

static shared as score_t bonus_states(2, 255)
static shared as uinteger bonus_index(255)

sub fuzzy_init_bonus_tables()
	'' row 0 is left as all zeros (default array value)

	'' row 1
	bonus_states(1, asc("/")) = SCORE_MATCH_SLASH
	bonus_states(1, asc("-")) = SCORE_MATCH_WORD
	bonus_states(1, asc("_")) = SCORE_MATCH_WORD
	bonus_states(1, asc(" ")) = SCORE_MATCH_WORD
	bonus_states(1, asc(".")) = SCORE_MATCH_DOT

	'' row 2
	bonus_states(2, asc("/")) = SCORE_MATCH_SLASH
	bonus_states(2, asc("-")) = SCORE_MATCH_WORD
	bonus_states(2, asc("_")) = SCORE_MATCH_WORD
	bonus_states(2, asc(" ")) = SCORE_MATCH_WORD
	bonus_states(2, asc(".")) = SCORE_MATCH_DOT

	'' ['a' ... 'z'] = SCORE_MATCH_CAPITAL   (row 2)
	for i as integer = asc("a") to asc("z")
		bonus_states(2, i) = SCORE_MATCH_CAPITAL
	next

	'' bonus_index: ['A' ... 'Z'] = 2
	for i as integer = asc("A") to asc("Z")
		bonus_index(i) = 2
	next

	'' ['a' ... 'z'] = 1
	for i as integer = asc("a") to asc("z")
		bonus_index(i) = 1
	next

	'' ['0' ... '9'] = 1
	for i as integer = asc("0") to asc("9")
		bonus_index(i) = 1
	next
end sub

#define fuzzy_compute_bonus(last_ch, ch) bonus_states(bonus_index((ch) and 255), (last_ch) and 255)
'#define max(a, b) iif((a) > (b), (a), (b))


function fuzzy_has_match( byval needle as DWSTRING, byval haystack as DWSTRING ) as boolean
	'' Case-insensitive, order-preserving subsequence test. Lowercasing
	'' both strings once and scanning with Instr replaces the original
	'' strcasechr()-based pointer walk.
	dim as DWSTRING lneedle = lcase(needle)
	dim as DWSTRING lhaystack = lcase(haystack)
	dim as integer hpos = 1

	for i as integer = 1 to len(lneedle)
		hpos = instr(hpos, lhaystack, mid(lneedle, i, 1))
		if hpos = 0 then return false
		hpos += 1
	next
	return true
end function

private sub fuzzy_precompute_bonus( byref haystack as DWSTRING, match_bonus() as score_t )
	'' Which positions are beginning of words
	dim as ubyte last_ch = asc("/")
	dim as ubyte ch
	dim as integer hlen = len(haystack)

	redim match_bonus(hlen - 1)

	for i as integer = 0 to hlen - 1
		ch = haystack[i]
		match_bonus(i) = fuzzy_compute_bonus(last_ch, ch)
		last_ch = ch
	next
end sub

private sub fuzzy_setup_match_struct( byref m as fuzzy_match_struct, byref needle as DWSTRING, byref haystack as DWSTRING )
	m.needle_len = len(needle)
	m.haystack_len = len(haystack)

	if m.needle_len > m.haystack_len then exit sub

	m.lower_needle = lcase(needle)
	m.lower_haystack = lcase(haystack)

	fuzzy_precompute_bonus( haystack, m.match_bonus() )
end sub

'' Fills row "row" (needle index i) of the D/M score matrices. Row i
'' reads row i-1 for its "previous row" values, so both fuzzy_match() and
'' fuzzy_match_postions() can share this one implementation now that the
'' matrices are plain 2D dynamic arrays instead of rotating buffers.
private sub fuzzy_match_row( byref m as fuzzy_match_struct, byval row as integer, _
                                d() as score_t, mm() as score_t )

	dim as integer n = m.needle_len
	dim as integer hlen = m.haystack_len
	dim as integer i = row

	dim as score_t prev_score = SCORE_MIN
	dim as score_t gap_score = iif(i = n - 1, SCORE_GAP_TRAILING, SCORE_GAP_INNER)

	dim as score_t prev_m = SCORE_MIN
	dim as score_t prev_d = SCORE_MIN
	dim as score_t score

	for j as integer = 0 to hlen - 1
		if m.lower_needle[i] = m.lower_haystack[j] then
			score = SCORE_MIN
			if i = 0 then
				score = (j * SCORE_GAP_LEADING) + m.match_bonus(j)
			elseif j > 0 then '' i > 0 and j > 0
				score = max(prev_m + m.match_bonus(j), _
				            /' consecutive match, doesn't stack with match_bonus '/ _
				            prev_d + SCORE_MATCH_CONSECUTIVE)
			end if
			if i > 0 then
				prev_d = d(i - 1, j)
				prev_m = mm(i - 1, j)
			end if
			d(i, j) = score
			prev_score = max(score, prev_score + gap_score)
			mm(i, j) = prev_score
		else
			if i > 0 then
				prev_d = d(i - 1, j)
				prev_m = mm(i - 1, j)
			end if
			d(i, j) = SCORE_MIN
			prev_score = prev_score + gap_score
			mm(i, j) = prev_score
		end if
	next
end sub

function fuzzy_match( byval needle as DWSTRING, byval haystack as DWSTRING ) as score_t
	if len(needle) = 0 then return SCORE_MIN

    static bonus_initialized as boolean = false
    if bonus_initialized = false then
        fuzzy_init_bonus_tables()
        bonus_initialized = true
    end if

	dim as fuzzy_match_struct m
	fuzzy_setup_match_struct(m, needle, haystack)

	dim as integer n = m.needle_len
	dim as integer hlen = m.haystack_len

	if n > hlen then
		'' No possible match.
		return SCORE_MIN
	elseif n = hlen then
		'' Since this method can only be called with a haystack which
		'' matches needle, if the lengths of the strings are equal the
		'' strings themselves may also be equal (ignoring case).
        if m.lower_needle = m.lower_haystack then
            return SCORE_MAX
        end if
	end if

	'' D() Stores the best score for this position ending with a match.
	'' M() Stores the best possible score at this position.
	'' Both are true 2D dynamic arrays, sized to the actual lengths --
	'' no MATCH_MAX_LEN cap and nothing to free().
	dim as score_t d(n - 1, hlen - 1)
	dim as score_t mm(n - 1, hlen - 1)

	for i as integer = 0 to n - 1
		fuzzy_match_row(m, i, d(), mm())
	next

	return mm(n - 1, hlen - 1)
end function

function fuzzy_match_postions( byval needle as DWSTRING, byval haystack as DWSTRING, positions() as uinteger ) as score_t
	if len(needle) = 0 then return SCORE_MIN

    static bonus_initialized as boolean = false
    if bonus_initialized = false then
        fuzzy_init_bonus_tables()
        bonus_initialized = true
    end if

	dim as fuzzy_match_struct m
	fuzzy_setup_match_struct(m, needle, haystack)

	dim as integer n = m.needle_len
	dim as integer hlen = m.haystack_len

	if n > hlen then
		'' No possible match.
		return SCORE_MIN
	elseif n = hlen then
		'' Since this method can only be called with a haystack which
		'' matches needle, if the lengths of the strings are equal the
		'' strings themselves may also be equal (ignoring case).
        if m.lower_needle = m.lower_haystack then
            redim positions(n - 1)
            for i as integer = 0 to n - 1
                positions(i) = i
            next
            return SCORE_MAX
        end if
	end if

	'' D()/M(): 2D dynamic arrays, dim'd directly to (n, hlen) at
	'' runtime. This replaces the malloc'd flat score_t[n][MATCH_MAX_LEN]
	'' blocks and the row*MATCH_MAX_LEN + col indexing from the C-style
	'' version -- (i, j) indexing works exactly like the mental model
	'' the flat indexing was standing in for.
	dim as score_t d(n - 1, hlen - 1)
	dim as score_t mm(n - 1, hlen - 1)

	for i as integer = 0 to n - 1
		fuzzy_match_row(m, i, d(), mm())
	next

	'' backtrace to find the positions of optimal matching
	redim positions(n - 1)
	dim as integer match_required = 0
	dim as integer j = hlen - 1

	for i as integer = n - 1 to 0 step -1
		do while j >= 0
			'' There may be multiple paths which result in the
			'' optimal weight. For simplicity, we will pick the
			'' first one we encounter, the latest in the
			'' candidate string.
			if ( d(i, j) <> SCORE_MIN ) andalso _
			   (match_required or d(i, j) = mm(i, j)) then

				'' If this score was determined using
				'' SCORE_MATCH_CONSECUTIVE, the previous
				'' character MUST be a match
				if (i > 0) andalso (j > 0) then
					match_required = (mm(i, j) = d(i - 1, j - 1) + SCORE_MATCH_CONSECUTIVE)
				else
					match_required = 0
				end if

				positions(i) = j
				j -= 1
				exit do
			end if
			j -= 1
		loop
	next 

	dim as score_t result = mm(n - 1, hlen - 1)

	return result
end function
