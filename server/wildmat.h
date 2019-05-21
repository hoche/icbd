/*  $Revision: 1.1.1.1 $
**
**  Do shell-style pattern matching for ?, \, [], and * characters.
**  Might not be robust in face of malformed patterns; e.g., "foo[a-"
**  could cause a segmentation violation.  It is 8bit clean.
**
**  Written by Rich $alz, mirror!rs, Wed Nov 26 19:03:17 EST 1986.
**  Rich $alz is now <rsalz@osf.org>.
**  April, 1991:  Replaced mutually-recursive calls with in-line code
**  for the star character.
**
*/

#ifndef _WILDMAT_H_
#define _WILDMAT_H_

/*
**  User-level routine.  Returns TRUE or FALSE.
*/
int wildmat(char *text, char *p);

#endif /* #ifndef _WILDMAT_H_ */
