/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	     	 - DOS Charset Recoder -				 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _RECODE_H
#define _RECODE_H

void       recode_ibmpc_to_latin1                (char *string, int len);
void       recode_latin1_to_ibmpc                (char *string, int len);

#endif /* _RECODE_H */
