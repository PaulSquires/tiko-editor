'' Target for the array half of the engine. Every shape the child walk has to cope with:
'' fixed 1-D and 2-D, dynamic 1-D and 2-D, a non-zero lower bound, an array of UDTs, and one
'' large enough that the old 1000-element display cap would have hidden most of it.
''
'' Build:  fbc64 -g -gen gas64 _testfile_arrays.bas
'' Run:    tiko_dbgtest _testfile_arrays.exe vars

type PT
    x as long
    y as long
end type

dim shared as long   gFixed1(0 to 4)
dim shared as long   gFixed2(1 to 3, 0 to 2)
dim shared as PT     gRecs(0 to 2)
dim shared as single gBig(0 to 9999)

sub Work()
    dim as double  loc1(10 to 14)
    redim as long  dyn1(0 to 6)
    redim as long  dyn2(0 to 2, 0 to 3)
    dim as long    plain = 7

    for i as long = 0 to 4      : gFixed1(i) = i * 10          : next
    for i as long = 1 to 3
        for j as long = 0 to 2  : gFixed2(i, j) = i * 100 + j  : next
    next
    for i as long = 0 to 2      : gRecs(i).x = i : gRecs(i).y = -i : next
    for i as long = 0 to 9999   : gBig(i) = i / 4.0            : next
    for i as long = 10 to 14    : loc1(i) = i + 0.5            : next
    for i as long = 0 to 6      : dyn1(i) = 1000 + i           : next
    for i as long = 0 to 2
        for j as long = 0 to 3  : dyn2(i, j) = i * 10 + j      : next
    next

    plain += 1                  '' <-- a line to stop on with everything populated
    print plain
end sub

Work()
