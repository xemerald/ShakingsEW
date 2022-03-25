/* Standard C header include */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
/* Local header include */
#include <tracepeak.h>
#include <triglist.h>
#include <geogfunc.h>
#include <shakemap.h>
#include <shakemap_misc.h>
#include <shakemap_triglist.h>

/* Local function prototype */
static void update_pvalue( const void * );


static STA_NODE       *Head = NULL;      /* First pointer of the linked list. */
static STA_NODE       *Tail = NULL;      /* Last pointer of the linked list. */
static uint32_t        ListLength = 0;   /* Length of the linked list. */
static volatile time_t TimeBaseNow = 0;

/*
 * shakemap_tlist_insert() - Search the key in the linked list, when there
 *                           is not, insert it.
 * Arguments:
 *   key   = Key to search.
 *   first = First pointer of the linked list.
 * Returns:
 *    NULL = Something error.
 *   !NULL = The pointer of the key.
 */
STA_NODE *shakemap_tlist_insert( const _STAINFO *key )
{
	STA_NODE  *new;
	STA_NODE **current = &Head;

/* */
	if ( current == NULL )
		return NULL;
/* */
	while ( *current != NULL ) {
		if ( shakemap_misc_snl_compare( (*current)->staptr, key ) == 0 )
			return *current;
		current = &(*current)->next;
	}
/* */
	new = calloc(1, sizeof(STA_NODE));
	if ( new != NULL ) {
		*current = new;
		new->staptr    = (_STAINFO *)key;
		new->shakeinfo = key->shakeinfo[key->shakelatest];
		new->next      = NULL;
		ListLength++;
	}
/* */
	if ( new->next == NULL )
		Tail = new;

	return new;
}

/*
 * shakemap_tlist_find() - Search the key in the linked list, when there
 *                          is not, insert it.
 * Arguments:
 *   key   = Key to find.
 *   first = First pointer of the linked list.
 * Returns:
 *    NULL = No result or something error.
 *   !NULL = The pointer of the key.
 */
STA_NODE *shakemap_tlist_find( const _STAINFO *key )
{
	STA_NODE **current = &Head;

/* */
	if ( current == NULL )
		return NULL;
/* */
	while ( *current != NULL ) {
		if ( shakemap_misc_snl_compare( (*current)->staptr, key ) == 0 )
			return *current;
		current = &(*current)->next;
	}

	return NULL;
}

/*
 * shakemap_tlist_delete() - Search the key in the linked list, when there
 *                            is not, insert it.
 * Arguments:
 *   key   = Key to delete.
 *   first = First pointer of the linked list.
 * Returns:
 *   None.
 */
STA_NODE *shakemap_tlist_delete( const _STAINFO *key )
{
	STA_NODE **current = &Head;
	STA_NODE *prev = (STA_NODE *)1;
	STA_NODE *next;

/* */
	if ( current == NULL && *current == NULL )
		return NULL;
/* */
	while ( shakemap_misc_snl_compare( (*current)->staptr, key ) != 0 ) {
		prev    = *current;
		current = &(*current)->next;
		if ( *current == NULL )
			return NULL;
	}
/* */
	next = (*current)->next;
	free(*current);
	*current = next;
/* */
	ListLength--;

	return prev;
}

/*
 * shakemap_tlist_pvalue_update() - Search the key in the linked list, when there
 *                                  is not, insert it.
 * Arguments:
 *   new    = Key to delete.
 *   target = First pointer of the linked list.
 * Returns:
 *   None.
 */
void shakemap_tlist_pvalue_update( void )
{
	shakemap_tlist_walk( update_pvalue );

	return;
}

/*
 * shakemap_tlist_len_get() - Search the key in the linked list, when there
 *                              is not, insert it.
 * Arguments:
 *   None.
 * Returns:
 *   >= 0 = The length of the trigger list.
 */
int shakemap_tlist_len_get( void )
{
	int       count   = 0;
	STA_NODE *current = Head;

/* */
	while ( current != NULL ) {
		count++;
		current = current->next;
	}

	return count;
}

/*
 * shakemap_tlist_walk() - Search the key in the linked list, when there
 *                         is not, insert it.
 * Arguments:
 *   action =
 * Returns:
 *   None.
 */
void shakemap_tlist_walk( void (*action)( const void * ) )
{
	STA_NODE *current = Head;

	if ( action == NULL ) return;

	while ( current != NULL ) {
		(*action)( current );
		current = current->next;
	}

	return;
}

/*
 * shakemap_tlist_time_sync() - Search the key in the linked list, when there
 *                              is not, insert it.
 * Arguments:
 *   timebase = Key to delete.
 * Returns:
 *   None.
 */
void shakemap_tlist_time_sync( const time_t timebase )
{
	STA_NODE *current = Head;
	int       shakeidx;

/* */
	while ( current != NULL ) {
	/* */
		shakeidx = current->staptr->shakelatest;
		while ( current->shakeinfo.peaktime > (timebase + 1.0) ) {
		/* */
			if ( --shakeidx < 0 )
				shakeidx = (int)SHAKE_BUF_LEN - 1;
			current->shakeinfo = current->staptr->shakeinfo[shakeidx];
		/* */
			if ( shakeidx == current->staptr->shakelatest ) {
				shakemap_tlist_delete( current->staptr );
				break;
			}
		}
		current = current->next;
	}

	TimeBaseNow = timebase;

	return;
}

/*
 * shakemap_tlist_pack() - Search the key in the linked list, when there
 *                         is not, insert it.
 * Arguments:
 *   key   = Key to delete.
 *   first = First pointer of the linked list.
 * Returns:
 *   None.
 */
int shakemap_tlist_pack( void *msg, size_t maxsize )
{
	int i = 0;

	STA_NODE           *current      = Head;
	SHAKE_LIST_HEADER  *slh          = (SHAKE_LIST_HEADER *)msg;
	SHAKE_LIST_ELEMENT *sle          = (SHAKE_LIST_ELEMENT *)(slh + 1);
	SHAKE_LIST_ELEMENT *const msgend = (SHAKE_LIST_ELEMENT *)((uint8_t *)msg + maxsize);

/* */
	while ( current != NULL ) {
		if ( (sle + 1) >= msgend )
			return -2;

		sle->staptr    = current->staptr;
		sle->shakeinfo = current->shakeinfo;

		i++;
		sle++;
		current = current->next;
	}

	slh->totalstations = i;

	return (sizeof(SHAKE_LIST_HEADER) + i*sizeof(SHAKE_LIST_ELEMENT));
}

/*
 * shakemap_tlist_destroy() - Search the key in the linked list, when there
 *                             is not, insert it.
 * Arguments:
 *   first = First pointer of the linked list.
 * Returns:
 *   None.
 */
void shakemap_tlist_destroy( void )
{
	STA_NODE *current = Head;
	STA_NODE *next;

/* */
	while ( current != NULL ) {
		next = current->next;
		free(current);
		current = next;
	}
/* */
	Head = Tail = NULL;
	ListLength = 0;

	return;
}



/*
*/
static void update_pvalue( const void *node )
{
	int          shakeidx   = ((STA_NODE *)node)->staptr->shakelatest;
	STA_SHAKE   *shake_save = &((STA_NODE *)node)->shakeinfo;
	STA_SHAKE   *shake_new  = &((STA_NODE *)node)->staptr->shakeinfo[shakeidx];
	const double shaketbase = shake_save->peaktime;

/* */
	while ( shake_new->peaktime > (double)(TimeBaseNow + 1) ) {
	/* */
		if ( --shakeidx < 0 )
			shakeidx = (int)SHAKE_BUF_LEN - 1;
		shake_new = &((STA_NODE *)node)->staptr->shakeinfo[shakeidx];
	/* */
		if ( shakeidx == ((STA_NODE *)node)->staptr->shakelatest )
			return;
	}
/* */
	while ( shake_new->peaktime > shaketbase ) {
	/* */
		if ( shake_new->peakvalue > shake_save->peakvalue )
			*shake_save = *shake_new;
	/* */
		if ( --shakeidx < 0 )
			shakeidx = (int)SHAKE_BUF_LEN - 1;
		if ( shakeidx == ((STA_NODE *)node)->staptr->shakelatest )
			break;
		shake_new = &((STA_NODE *)node)->staptr->shakeinfo[shakeidx];
	}

	return;
}
