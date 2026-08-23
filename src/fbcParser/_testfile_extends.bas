'' Parser test INPUT (never compiled by build.bat) - EXTENDS base exposure.
'' A TYPE/UNION record's typeOffset must name its EXTENDS base in original
'' source casing; -1 when the type has no base.
''
'' Expected:
''   Animal   TYPE   typeText <none>
''   Dog      TYPE   typeText "Animal"
''   Wolf     TYPE   typeText "Dog"
''   Critter  TYPE   typeText "OBJECT"
''   Blob     UNION  typeText <none>

type Animal
    legCount as long
    declare sub Speak( )
end type

sub Animal.Speak( )
    print "..."
end sub

type Dog extends Animal
    kennelName as string
end type

type Wolf extends Dog
    packSize as long
end type

type Critter extends object
    id as long
end type

union Blob
    a as long
    b as single
end union

dim shared gDog as Dog
dim shared gWolf as Wolf
dim shared gCritter as Critter
dim shared gBlob as Blob
