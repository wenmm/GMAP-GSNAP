/* $Id: oligoindex.h,v 1.23 2005/10/01 05:13:00 twu Exp $ */
#ifndef OLIGOINDEX_INCLUDED
#define OLIGOINDEX_INCLUDED
#include "bool.h"
#include "sequence.h"

#define T Oligoindex_T
typedef struct T *T;

extern void
Oligoindex_init (int indexsize0);
extern T
Oligoindex_new (void);
extern double
Oligoindex_set_inquery (int *badoligos, int *trim_start, int *trim_end, T this, Sequence_T queryuc);
extern void
Oligoindex_tally (T this, Sequence_T genomicuc);
extern void
Oligoindex_cleanup (T this, Sequence_T queryuc);
extern void
Oligoindex_free (T *old);

extern unsigned int **
Oligoindex_get_mappings (int **npositions, int *totalpositions, T this, 
			 Sequence_T queryuc);


#undef T
#endif

