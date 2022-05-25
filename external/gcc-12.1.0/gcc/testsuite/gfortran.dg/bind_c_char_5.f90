! { dg-do run }
! { dg-additional-options "-fcheck=all" }
! { dg-shouldfail "Substring out of bounds" }
!
! PR fortran/85781
!
! Co-contributed by G. Steinmetz 

  use iso_c_binding, only: c_char
  call s(c_char_'x', -2, -2)
contains
  subroutine s(x,m,n) bind(c)
    use iso_c_binding, only: c_char
    character(kind=c_char), value :: x
    call foo(x(m:), m, n)
  end
  subroutine foo(str, m, n)
    character(len=*) :: str
  end
end
! { dg-output "Fortran runtime error: Substring out of bounds: lower bound .-2. of 'x' is less than one" }
