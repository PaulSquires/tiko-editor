'' Field reference paths. Names encode expected counts (_R<n>W<n>).
type Animal
    inherited_R1W1 as long
end type

type Pet extends Animal
    arr_R1W1(0 to 3) as long
    scalar_R1W1      as long
    implicitW_R0W1   as long
    implicitR_R1W0   as long
    withField_R1W1   as long
    ctorField_R0W1   as long
    ptrField_R1W1    as long
    ghost_R0W0       as long
    declare constructor( )
    declare sub Feed( byval i as long )
    declare function Total( ) as long
end type

constructor Pet( )
    ctorField_R0W1 = 0
end constructor

sub Pet.Feed( byval i as long )
    this.arr_R1W1(i) = 1
    this.scalar_R1W1 = 2
    implicitW_R0W1 = 3
    this.inherited_R1W1 = 4
    with this
        .withField_R1W1 = 5
    end with
end sub

function Pet.Total( ) as long
    dim as long n = this.arr_R1W1(0) + this.scalar_R1W1 + implicitR_R1W0 + this.inherited_R1W1
    with this
        n += .withField_R1W1
    end with
    return n
end function

dim as Pet p
dim as Pet ptr pp = @p
pp->ptrField_R1W1 = 9
print pp->ptrField_R1W1
p.Feed( 1 )
print p.Total()
