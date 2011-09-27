/* $Id: pairpool.h,v 1.9 2005/02/15 01:58:50 twu Exp $ */
#ifndef PAIRPOOL_INCLUDED
#define PAIRPOOL_INCLUDED
#include "pair.h"
#include "list.h"

#define T Pairpool_T
typedef struct T *T;

extern void
Pairpool_free (T *old);
extern T
Pairpool_new ();
extern void
Pairpool_reset (T this);
extern List_T
Pairpool_push (List_T list, T this, int querypos, int genomepos, char cdna, char comp, char genome);
extern List_T
Pairpool_push_existing (List_T list, T this, Pair_T pair);
extern List_T
Pairpool_pop (List_T list, Pair_T *x);
extern List_T
Pairpool_transfer (List_T dest, List_T source);
extern List_T
Pairpool_transfer_copy (List_T dest, List_T source, T this);
extern List_T
Pairpool_transfer_copy_bounded (List_T dest, List_T source, T this, int minpos, int maxpos);

#undef T
#endif