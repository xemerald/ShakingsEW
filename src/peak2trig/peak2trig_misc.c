/* */
#include <string.h>
/* */
#include <trace_buf.h>
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

	if ( (rc = memcmp(tmpa->sta, tmpb->sta, TRACE2_STA_LEN)) != 0 )
		return rc;
	if ( (rc = memcmp(tmpa->net, tmpb->net, TRACE2_NET_LEN)) != 0 )
		return rc;
	return memcmp(tmpa->loc, tmpb->loc, TRACE2_LOC_LEN);
}
