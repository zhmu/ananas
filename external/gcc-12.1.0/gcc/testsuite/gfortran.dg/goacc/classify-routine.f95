! Check offloaded function's attributes and classification for OpenACC
! routine.

! { dg-additional-options "-O2" }
! { dg-additional-options "-fopt-info-optimized-omp" }
! { dg-additional-options "-fdump-tree-ompexp" }
! { dg-additional-options "-fdump-tree-oaccloops" }

! { dg-additional-options "-Wopenacc-parallelism" } for testing/documenting
! aspects of that functionality.

subroutine ROUTINE
  !$acc routine worker
  integer, parameter :: n = 1024
  integer, dimension (0:n-1) :: a, b, c
  integer :: i

  call setup(a, b)

  !$acc loop ! { dg-line l_loop_i1 }
  ! { dg-optimized {assigned OpenACC worker vector loop parallelism} {} { target *-*-* } l_loop_i1 }
  do i = 0, n - 1
     c(i) = a(i) + b(i)
  end do
end subroutine ROUTINE

! Check the offloaded function's attributes.
! { dg-final { scan-tree-dump-times "(?n)__attribute__\\(\\(oacc function \\(0 1, 1 0, 1 0\\), omp declare target \\(worker\\)\\)\\)" 1 "ompexp" } }

! Check the offloaded function's classification and compute dimensions (will
! always be 1 x 1 x 1 for non-offloading compilation).
! { dg-final { scan-tree-dump-times "(?n)Function is OpenACC routine level 1" 1 "oaccloops" } }
! { dg-final { scan-tree-dump-times "(?n)OpenACC routine 'routine' doesn't have 'nohost' clause" 1 "oaccloops" { target { ! offloading_enabled } } } }
! { dg-final { scan-tree-dump-times "(?n)OpenACC routine 'routine_' doesn't have 'nohost' clause" 1 "oaccloops" { target offloading_enabled } } }
! { dg-final { scan-tree-dump-times "(?n)OpenACC routine 'routine' not discarded" 1 "oaccloops" { target { ! offloading_enabled } } } }
! { dg-final { scan-tree-dump-times "(?n)OpenACC routine 'routine_' not discarded" 1 "oaccloops" { target offloading_enabled } } }
! { dg-final { scan-tree-dump-times "(?n)Compute dimensions \\\[1, 1, 1\\\]" 1 "oaccloops" } }
! { dg-final { scan-tree-dump-times "(?n)__attribute__\\(\\(oacc function \\(0 1, 1 1, 1 1\\), omp declare target \\(worker\\)\\)\\)" 1 "oaccloops" } }
! { dg-final { scan-tree-dump-times "(?n)void routine \\(\\)" 1 "oaccloops" { target { ! offloading_enabled } } } }
! { dg-final { scan-tree-dump-times "(?n)void routine_ \\(\\)" 1 "oaccloops" { target offloading_enabled } } }
!TODO See PR101551 for 'offloading_enabled' differences.
