/* */
#include <string.h>
/* */
#include <peak2trig.h>

/*
 * peak2trig_misc_snl_compare() - the SCNL compare function of binary tree search
 */
int peak2trig_misc_snl_compare( const void *a, const void *b )
{
	_STAINFO *tmpa = (_STAINFO *)a;
	_STAINFO *tmpb = (_STAINFO *)b;
	int       rc;

	if ( (rc = strcmp(tmpa->sta, tmpb->sta)) != 0 )
		return rc;
	if ( (rc = strcmp(tmpa->net, tmpb->net)) != 0 )
		return rc;
	return strcmp(tmpa->loc, tmpb->loc);
}
