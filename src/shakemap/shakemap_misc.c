/* */
#include <string.h>
/* */
#include <shakemap.h>

/*
 * shakemap_misc_snl_compare() - the SNL compare function of binary tree search
 */
int shakemap_misc_snl_compare( const void *a, const void *b )
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
