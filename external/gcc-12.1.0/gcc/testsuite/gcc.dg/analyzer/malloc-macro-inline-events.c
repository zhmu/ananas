/* Test path-printing in the face of macros.  */

/* { dg-additional-options "-fdiagnostics-show-line-numbers -fdiagnostics-path-format=inline-events -fdiagnostics-show-caret" } */
/* { dg-enable-nn-line-numbers "" } */

#include "malloc-macro.h"

/* { dg-warning "double-'free' of 'ptr'" "" { target *-*-* } 2 } */

int test (void *ptr)
{
  WRAPPED_FREE (ptr); /* { dg-message "in expansion of macro 'WRAPPED_FREE'" } */
  WRAPPED_FREE (ptr); /* { dg-message "in expansion of macro 'WRAPPED_FREE'" } */

  /* Erase the spans indicating the header file
     (to avoid embedding path assumptions).  */
  /* { dg-regexp "\[^|\]+/malloc-macro.h:\[0-9\]+:\[0-9\]+:" } */
  /* { dg-regexp "\[^|\]+/malloc-macro.h:\[0-9\]+:\[0-9\]+:" } */

  /* { dg-begin-multiline-output "" }
   NN | #define WRAPPED_FREE(PTR) free(PTR)
      |                           ^~~~~~~~~
   NN |   WRAPPED_FREE (ptr);
      |   ^~~~~~~~~~~~
  'test': event 1
    |
    |
    |   NN | #define WRAPPED_FREE(PTR) free(PTR)
    |      |                           ^~~~~~~~~
    |      |                           |
    |      |                           (1) first 'free' here
    |   NN |   WRAPPED_FREE (ptr);
    |      |   ^~~~~~~~~~~~
    |
  'test': event 2
    |
    |
    |   NN | #define WRAPPED_FREE(PTR) free(PTR)
    |      |                           ^~~~~~~~~
    |      |                           |
    |      |                           (2) second 'free' here; first 'free' was at (1)
    |   NN |   WRAPPED_FREE (ptr);
    |      |   ^~~~~~~~~~~~
    |
     { dg-end-multiline-output "" } */
}
