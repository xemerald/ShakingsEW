/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <float.h>
#include <cpgplot.h>

/* Earthworm environment header include */
#include <earthworm.h>

/* Local header include */
#include <datestring.h>
#include <griddata.h>
#include <shakeint.h>
#include <recordtype.h>
#include <postshake.h>
#include <postshake_plot.h>
#include <postshake_misc.h>

#define  COLOR_INDEX_BASE  30
#define  COLOR_INDEX_LGRAY 17
#define  COLOR_INDEX_BG    0
#define  COLOR_INDEX_FG    1

typedef struct {
	uint8_t   numofgirds;
	GRID_REC *gridptr[MAX_IN_MSG];
} FUSION_GRID;

static int  plot_shakemap_pgplot( void *, const char *, const char * );
static int  link_fusion_grid( const void *, FUSION_GRID **, FUSION_GRID ** );
static void plot_basemap_fancy( const float, const float, const float, const float, const char *, const char *, const char * );
static void plot_colorbar( const float, const float, const short );
static void plot_gridline( const float, const float, const float, const float );
static void plot_mapgrids( const FUSION_GRID *, const short );
//static void plot_mapgrids( const float, const float, const double, const short );
static void free_polyline( POLY_LINE * );

static float      MapRange[4];
static POLY_LINE *NormalPLine = NULL;
static POLY_LINE *StrongPLine = NULL;

static const float ColorIndexOcta[][3] = { { 0.1875, 1.0000, 0.8125 },
											{ 0.5625, 1.0000, 0.4375 },
											{ 0.7500, 1.0000, 0.2500 },
											{ 0.9375, 1.0000, 0.0625 },
											{ 1.0000, 0.6875, 0.0000 },
											{ 1.0000, 0.3125, 0.0000 },
											{ 1.0000, 0.1250, 0.0000 },
											{ 0.8750, 0.0000, 0.0000 } };

static const float ColorIndexDeca[][3] = { { 0.8549, 1.0000, 0.8196 },
											{ 0.7333, 1.0000, 0.6627 },
											{ 0.5608, 1.0000, 0.4353 },
											{ 0.7490, 1.0000, 0.2471 },
											{ 0.9373, 1.0000, 0.0588 },
											{ 1.0000, 0.6863, 0.0000 },
											{ 1.0000, 0.3098, 0.0000 },
											{ 1.0000, 0.1216, 0.0000 },
											{ 0.8745, 0.0000, 0.0000 },
											{ 0.4980, 0.0000, 0.0000 } };



/*
 * psk_plot_init( ) -- Initialization function of Plotting.
 * Arguments:
 *   ringName   = Name of list ring.
 *   modId      = The module ID of calling process.
 *   ringSwitch = The switch for getting list from ring.
 * Returns:
 *    0 = Normal.
 */
int psk_plot_init( const float minlon, const float maxlon, const float minlat, const float maxlat )
{
	int result;

	MapRange[0] = minlon;
	MapRange[1] = maxlon;
	MapRange[2] = minlat;
	MapRange[3] = maxlat;

	if ( (MapRange[1] - MapRange[0]) <= 0.0 || (MapRange[3] - MapRange[2]) <= 0.0 ) {
		logit( "e", "postshake: Setting range of map error, please check it!\n" );
		result = -1;
	}
	else {
		logit( "o", "postshake: Range of map is longitude: %6.2f-%6.2f, latitude: %5.2f-%5.2f\n",
		MapRange[0], MapRange[1], MapRange[2], MapRange[3]);
		result = 0;
	}

	return result;
}

/*
 * psk_plot_sm_plot() - Read the station info from the array <stainfo>. And creates
 *                      the table of postshake of Taiwan.
 */
int psk_plot_sm_plot( void *psm, const char *reportPath, const char *resfilename )
{
	return plot_shakemap_pgplot( psm, reportPath, resfilename );
}

/*
 *
 */
int psk_plot_polyline_read( const char *plfilename, int plinetype )
{
	int    i;
	int    pointcount;
	float *tmp_x = NULL;
	float *tmp_y = NULL;
	char   line[MAX_STR_SIZE];

	FILE      *fd;
	POLY_LINE *newpl  = NULL;
	POLY_LINE **plptr = NULL;

/* Initialization */
	tmp_x = calloc(60000, sizeof(float));
	tmp_y = calloc(60000, sizeof(float));

	if ( tmp_x == NULL || tmp_y == NULL ) {
		logit( "e", "postshake: Error allocate temporary memory for polygon line!\n" );
		return -1;
	}

	memset(line, 0, sizeof(line));

	if ( (fd = fopen(plfilename, "r")) == NULL ) {
		logit( "e", "postshake: Error opening polygon line file %s!\n", plfilename );
		return -1;
	}

	switch ( plinetype ) {
	case PLOT_NORMAL_POLY:
		plptr = &NormalPLine;
		break;
	case PLOT_STRONG_POLY:
		plptr = &StrongPLine;
		break;
	default:
		logit( "e", "postshake: Unknown type ot polygon line!\n" );
		return -2;
	}

	while ( *plptr != NULL ) plptr = &(*plptr)->next;
	pointcount = 0;

	while ( fgets(line, sizeof(line) - 1, fd) != NULL ) {
		if ( !strlen(line) ) continue;

		for ( i = 0; i < MAX_STR_SIZE; i++ ) {
			if ( line[i] == '#' || line[i] == '\n' ) break;
			else if ( line[i] == '\t' || line[i] == ' ' ) continue;
			else if ( line[i] == '>' ) {
				if ( pointcount > 0 ) {
					if ( (newpl = calloc(1, sizeof(POLY_LINE))) == NULL ) {
						logit( "e", "postshake: Error allocate memory for polygon line!\n" );
						free_polyline( *plptr );
						return -1;
					}
					newpl->points = pointcount;
					newpl->x      = calloc(pointcount + 1, sizeof(float));
					newpl->y      = calloc(pointcount + 1, sizeof(float));
					newpl->next   = NULL;
					memcpy(newpl->x, tmp_x, pointcount*sizeof(float));
					memcpy(newpl->y, tmp_y, pointcount*sizeof(float));

					*plptr = newpl;
					plptr  = &(*plptr)->next;
				}
				else break;

				pointcount = 0;
			}
			else {
				if ( sscanf( line, "%f %f", tmp_x + pointcount, tmp_y + pointcount ) != 2 ) {
					logit( "e", "postshake: Error reading boundary file %s!\n", plfilename );
					free_polyline( *plptr );
					return -1;
				}
				else pointcount++;
			}
			break;
		}
	}

	fclose(fd);
	free(tmp_x);
	free(tmp_y);

	return 0;
}

/*
 * psk_plot_end( ) -- End process of plot
 * Arguments:
 *   None.
 * Returns:
 *   None.
 */
void psk_plot_end( void )
{
	free_polyline( NormalPLine );
	free_polyline( StrongPLine );

	return;
}

/*
 * plot_shakemap_pgplot() Read the station info from the array <stainfo>. And creates
 *              the table of postshake of Taiwan.
 */
static int plot_shakemap_pgplot( void *psm, const char *reportpath, const char *resfilename )
{
	int i, trigstations = 0;

	PLOTSMAP       *psmptr = (PLOTSMAP *)psm;
	GRIDMAP_HEADER *gmh    = NULL;
	FUSION_GRID    *fmgrid, *fsgrid;
	FUSION_GRID    *fgp, *fge;

	POLY_LINE      *plptr  = NULL;

	const float mapsidelen[2]  = { MapRange[1] - MapRange[0], MapRange[3] - MapRange[2] };
	const float textpos[2]     = { MapRange[0] + mapsidelen[0]*0.064, MapRange[3] - mapsidelen[1]*0.064 };
	const float colorbarpos[2] = { MapRange[1] - mapsidelen[0]*0.16, MapRange[2] + mapsidelen[1]*0.05 };
	const float scaler         = mapsidelen[1] / mapsidelen[0];

	struct tm *tp = NULL;

	char tmpstring[MAX_PATH_STR];
	char stimestring[MAX_DSTR_LENGTH];
	char etimestring[MAX_DSTR_LENGTH];

/* Start plotting the shakemap */
	if ( (gmh = psk_misc_refmap_get( psmptr )) == NULL ) {
		logit( "e", "postshake: Can't find the grid data in the plotting shakemap structure!\n" );
		return -1;
	}

/* Generate the start & end timestamp string */
	tp = gmtime( &gmh->starttime );
	date2spstring( tp, stimestring, MAX_DSTR_LENGTH );
	tp = gmtime( &gmh->endtime );
	date2spstring( tp, etimestring, MAX_DSTR_LENGTH );

/* Generate the output figure name */
	sprintf(tmpstring, "%s%s/png", reportpath, resfilename);

/* Start the pgplot process, open the canva */
	if ( !cpgopen(tmpstring) ) {
		logit( "e", "postshake: Create shakemap image file error, please check the setting of path!\n" );
		return -1;
	}

/* Define the background & foreground color index */
	cpgscr(COLOR_INDEX_FG,    0.0, 0.0, 0.0);
	cpgscr(COLOR_INDEX_BG,    1.0, 1.0, 1.0);
	cpgscr(COLOR_INDEX_LGRAY, 0.9, 0.9, 0.9);

	switch ( shake_get_maxintensity( psmptr->smaptype ) ) {
	case SHAKE_INT_X:
		for ( i=0; i<10; i++ ) {
			cpgscr(COLOR_INDEX_BASE + i, ColorIndexDeca[i][0], ColorIndexDeca[i][1], ColorIndexDeca[i][2]);
		}
		break;
	case SHAKE_INT_VIII:
	default:
		for ( i=0; i<8; i++ ) {
			cpgscr(COLOR_INDEX_BASE + i, ColorIndexOcta[i][0], ColorIndexOcta[i][1], ColorIndexOcta[i][2]);
		}
		break;
	}

	cpgpap(30, scaler);
	sprintf(tmpstring, "%s", psmptr->title);

	cpgbbuf();
	cpgsci(COLOR_INDEX_FG);
	cpgslw(8);
	cpgsch(1.0);
	plot_basemap_fancy(MapRange[0], MapRange[1], MapRange[2], MapRange[3], "Longitude(\260E)", "Latitude(\260N)", tmpstring);

/* The plain style map frame */
	//cpgenv(MapRange[0], MapRange[1], MapRange[2], MapRange[3], 1, 1);
	//cpglab("Longitude(\260E)", "Latitude(\260N)", tmpstring);

	fmgrid = fsgrid = NULL;
	trigstations = link_fusion_grid( psmptr, &fmgrid, &fsgrid );
	if ( fmgrid == NULL || fsgrid == NULL || trigstations == 0 ) {
		logit( "e", "postshake: Parsing grid data error, please check the data source!\n" );
		return -1;
	}
/* */
	for ( fgp = fmgrid, fge = fgp + (gmh->totalgrids - trigstations); fgp < fge; fgp++ )
		plot_mapgrids( fgp, psmptr->smaptype );

	cpgslw(3);
	cpgsci(COLOR_INDEX_LGRAY);
/* Plotting the grid line on the map */
	plot_gridline( MapRange[0], MapRange[1], MapRange[2], MapRange[3] );

/* Drawing the colorbar */
	plot_colorbar( colorbarpos[0], colorbarpos[1], psmptr->smaptype );

/* Drawing normal polygon line */
	cpgslw(5);
	cpgsci(15);
	for ( plptr = NormalPLine; plptr != NULL; plptr = plptr->next )
		if ( plptr->points ) cpgline( plptr->points, plptr->x, plptr->y );

/* If define the strong polygon line, it will be emphasised here. */
	cpgslw(13);
	cpgsci(COLOR_INDEX_FG);
	for ( plptr = StrongPLine; plptr != NULL; plptr = plptr->next )
		if ( plptr->points ) cpgline( plptr->points, plptr->x, plptr->y );

/* Writing the time stamp and triggered station */
	cpgslw(11);
	cpgsch(1.0);
	cpgstbg(COLOR_INDEX_BG);
	sprintf(tmpstring, "From:");
	cpgtext(textpos[0], textpos[1], tmpstring);
	sprintf(tmpstring, "To:");
	cpgtext(textpos[0], textpos[1] - 0.11, tmpstring);
	cpgstbg(-1);
	sprintf(tmpstring, "%s(UTC)", stimestring);
	cpgtext(textpos[0] + 0.31, textpos[1], tmpstring);
	sprintf(tmpstring, "%s(UTC)", etimestring);
	cpgtext(textpos[0] + 0.31, textpos[1] - 0.11, tmpstring);
	sprintf(tmpstring, "Triggered station: %d", trigstations);
	cpgtext(textpos[0], textpos[1] - 0.22, tmpstring);
/* Writing the caption on the map */
	if ( strlen(psmptr->caption) ) {
		sprintf(tmpstring, "%s", psmptr->caption);
		cpgtext(textpos[0], textpos[1] - 0.33, tmpstring);
	}

/* Plotting the triggered stations. */
	cpgslw(4);
	cpgsch(1.0);
	for ( fgp = fsgrid, fge = fgp + trigstations; fgp < fge; fgp++ ) {
		cpgsci(2);
		cpgpt1((float)fgp->gridptr[0]->longitude, (float)fgp->gridptr[0]->latitude, -3);
		cpgsci(COLOR_INDEX_FG);
		cpgpt1((float)fgp->gridptr[0]->longitude, (float)fgp->gridptr[0]->latitude, 7);
	}

/* End the pgplot process */
	cpgebuf();
	cpgend();

/* Release the created fusion grids */
	free(fsgrid);
	free(fmgrid);
/* End plotting the shakemap */

	return trigstations;
}

/*
 *
 */
static void plot_basemap_fancy( const float minlon, const float maxlon, const float minlat, const float maxlat, const char *xlab, const char *ylab, const char *title )
{
/* Frame related variables */
	const float width = 0.032;
	const float minlon_o = minlon - width;
	const float maxlon_o = maxlon + width;
	const float minlat_o = minlat - width;
	const float maxlat_o = maxlat + width;
/* Tick-control variables */
	float tickpos, tmppos_l, tmppos_r;
/* Viewport setting related variables */
	float vpx1, vpx2, vpy1, vpy2;
	float vpoffset_x, vpoffset_y;


/* Plot the outter box */
	cpgbbuf();
	cpgenv(minlon_o, maxlon_o, minlat_o, maxlat_o, 0, -1);

/* Plot the inner box */
	cpgmove(minlon_o, minlat);
	cpgdraw(maxlon_o, minlat);
	cpgmove(minlon_o, maxlat);
	cpgdraw(maxlon_o, maxlat);
	cpgmove(minlon, minlat_o);
	cpgdraw(minlon, maxlat_o);
	cpgmove(maxlon, minlat_o);
	cpgdraw(maxlon, maxlat_o);

/* Get the current viewport position & viewport offset with 0.032(width of frame)*/
	cpgqvp(0, &vpx1, &vpx2, &vpy1, &vpy2);
	vpoffset_x = width/(maxlon_o - minlon_o) * (vpx2 - vpx1);
	vpoffset_y = width/(maxlat_o - minlat_o) * (vpy2 - vpy1);

/* Only extend the viewport in x direction(shrink in y direction) & plot the main tick */
	cpgsvp(vpx1, vpx2, vpy1 + vpoffset_y, vpy2 - vpoffset_y);
	cpgswin(minlon_o, maxlon_o, minlat, maxlat);
	for ( tickpos = minlat; tickpos <= maxlat; tickpos += 1.0 ) {
		tmppos_l = (int)tickpos;
		if ( (int)tmppos_l % 2 || (tmppos_l + 1.0) < minlat ) continue;
		else {
			tmppos_l = (tmppos_l < minlat) ? minlat : tmppos_l;
			tmppos_r = tmppos_l + 1.0;
			tmppos_r = (tmppos_r > maxlat) ? maxlat : tmppos_r;
			cpgrect(minlon_o, minlon, tmppos_l, tmppos_r);
			cpgrect(maxlon, maxlon_o, tmppos_l, tmppos_r);
		}
	}
/* Annotate the y axis */
	cpgbox("", 0.0, 0.0, "NV", 0.0, 0.0);

/* Only extend the viewport in y direction(shrink in x direction) & plot the main tick */
	cpgsvp(vpx1 + vpoffset_x, vpx2 - vpoffset_x, vpy1, vpy2);
	cpgswin(minlon, maxlon, minlat_o, maxlat_o);
	for ( tickpos = minlon; tickpos <= maxlon; tickpos += 1.0 ) {
		tmppos_l = (int)tickpos;
		if ( (int)tmppos_l % 2 || (tmppos_l + 1.0) < minlon ) continue;
		else {
			tmppos_l = (tmppos_l < minlon) ? minlon : tmppos_l;
			tmppos_r = tmppos_l + 1.0;
			tmppos_r = (tmppos_r > maxlon) ? maxlon : tmppos_r;
			cpgrect(tmppos_l, tmppos_r, minlat_o, minlat);
			cpgrect(tmppos_l, tmppos_r, maxlat, maxlat_o);
		}
	}
/* Annotate the x axis */
	cpgbox("N", 0.0, 0.0, "", 0.0, 0.0);

/* Change the viewport to fit the inner box */
	cpgswin(minlon, maxlon, minlat, maxlat);
	cpgsvp(vpx1 + vpoffset_x, vpx2 - vpoffset_x, vpy1 + vpoffset_y, vpy2 - vpoffset_y);

/* Annotate the labels of axis */
	if ( strlen(xlab) )  cpgmtxt("B", 3.0, 0.5, 0.5, xlab);
	if ( strlen(ylab) )  cpgmtxt("L", 3.0, 0.5, 0.5, ylab);
	if ( strlen(title) ) cpgmtxt("T", 1.0, 0.5, 0.5, title);
	cpgebuf();

	return;
}

/*
 *
 */
static int link_fusion_grid( const void *psm, FUSION_GRID **fgm, FUSION_GRID **fgs )
{
	int i;

	PLOTSMAP       *psmptr = (PLOTSMAP *)psm;
/* */
	GRIDMAP_HEADER *gmh    = NULL;
	GRID_REC       *gdr    = NULL;
	GRID_REC       *gde    = NULL;
/* */
	FUSION_GRID    *fmgrid = NULL;
	FUSION_GRID    *fsgrid = NULL;
	FUSION_GRID    *fgrptr = NULL;

	const int trigstations = psk_misc_trigstations_get( psmptr );

/**/
	for ( i = 0; i < MAX_IN_MSG; i++ ) {
		if ( psmptr->gmflag[i] ) {
			gmh = psmptr->gmptr[i];
			break;
		}
	}

/**/
	*fgs = fsgrid = (FUSION_GRID *)malloc(sizeof(FUSION_GRID) * trigstations);
	*fgm = fmgrid = (FUSION_GRID *)malloc(sizeof(FUSION_GRID) * (gmh->totalgrids - trigstations));

/**/
	for ( gdr = (GRID_REC *)(gmh + 1), gde = gdr + gmh->totalgrids; gdr < gde; gdr++ ) {
		if ( gdr->gridtype == GRID_STATION ) {
			fgrptr = fsgrid;
			fsgrid++;
		}
		else {
			fgrptr = fmgrid;
			fmgrid++;
		}
	/* */
		fgrptr->gridptr[0] = gdr;
		fgrptr->numofgirds = 1;
	}

/**/
	for ( i++; i < MAX_IN_MSG; i++ ) {
	/**/
		if ( psmptr->gmflag[i] ) {
		/**/
			gmh = psmptr->gmptr[i];
		/**/
			fsgrid = *fgs;
			fmgrid = *fgm;
		/**/
			for ( gdr = (GRID_REC *)(gmh + 1), gde = gdr + gmh->totalgrids; gdr < gde; gdr++ ) {
				if ( gdr->gridtype == GRID_STATION ) {
					//for ( fgrptr = fsgrid; fgrptr < *fgs + trigstations; fgrptr++ ) {
						//if ( strcmp(gdr->gridname, fgrptr->gridname) == 0 ) break;
					//}
					fgrptr = fsgrid;
					fsgrid++;
				}
				else {
					fgrptr = fmgrid;
					fmgrid++;
				}
				fgrptr->gridptr[fgrptr->numofgirds++] = gdr;
			}
		}
	}

	return trigstations;
}

/*
 *
 */
static void plot_mapgrids( const FUSION_GRID *fsgrid, const short smaptype )
{
	int    i;
	double gridvalue[fsgrid->numofgirds];
/* */
	for ( i = 0; i < fsgrid->numofgirds; i++ ) gridvalue[i] = fsgrid->gridptr[i]->gridvalue;

	const float lon = (float)fsgrid->gridptr[0]->longitude;
	const float lat = (float)fsgrid->gridptr[0]->latitude;

	cpgsci(shake_get_intensity( gridvalue, fsgrid->numofgirds, smaptype ) + COLOR_INDEX_BASE);
	cpgrect(lon-0.01, lon+0.04, lat-0.01, lat+0.04);

	return;
}

/*
 *
 */
static void plot_colorbar( const float lon, const float lat, const short smaptype )
{
	int i;

	cpgsave();
	cpgsci(COLOR_INDEX_FG);
	cpgslw(10);
	cpgsch(0.8);

	switch ( smaptype ) {
		case SHAKE_PAPGA:
			cpgtext(lon + 0.1, lat - 0.03, "0.0");
			cpgtext(lon + 0.1, lat + 0.87, "400.0");
			cpgtext(lon + 0.1, lat + 0.77, "250.0");
			cpgtext(lon + 0.1, lat + 0.67, "150.0");
			cpgtext(lon + 0.1, lat + 0.57, "80.0");
			cpgtext(lon + 0.1, lat + 0.47, "40.0");
			cpgtext(lon + 0.1, lat + 0.37, "25.0");
			cpgtext(lon + 0.1, lat + 0.27, "15.0");
			cpgtext(lon + 0.1, lat + 0.17, "8.0");
			cpgtext(lon + 0.1, lat + 0.07, "2.5");
			cpgtext(lon - 0.05, lat + 1.02, "PGA(gal)");

			for( i=0; i<10; i++) {
				cpgsci(COLOR_INDEX_BASE + i);
				cpgrect(lon, lon + 0.08, lat + i*0.1, lat + 0.08 + i*0.1);
			}
			break;
		case SHAKE_PAPGV:
			cpgtext(lon + 0.1, lat - 0.03, "0.0");
			cpgtext(lon + 0.1, lat + 0.87, "75.0");
			cpgtext(lon + 0.1, lat + 0.77, "49.0");
			cpgtext(lon + 0.1, lat + 0.67, "31.0");
			cpgtext(lon + 0.1, lat + 0.57, "17.0");
			cpgtext(lon + 0.1, lat + 0.47, "8.75");
			cpgtext(lon + 0.1, lat + 0.37, "5.70");
			cpgtext(lon + 0.1, lat + 0.27, "3.45");
			cpgtext(lon + 0.1, lat + 0.17, "1.90");
			cpgtext(lon + 0.1, lat + 0.07, "0.65");
			cpgtext(lon - 0.05, lat + 1.02, "PGV(cm/s)");

			for( i=0; i<10; i++) {
				cpgsci(COLOR_INDEX_BASE + i);
				cpgrect(lon, lon + 0.08, lat + i*0.1, lat + 0.08 + i*0.1);
			}
			break;
		case SHAKE_PASA:
			cpgtext(lon + 0.1, lat - 0.03, "0.0");
			cpgtext(lon + 0.1, lat + 0.87, "400.0");
			cpgtext(lon + 0.1, lat + 0.77, "250.0");
			cpgtext(lon + 0.1, lat + 0.67, "150.0");
			cpgtext(lon + 0.1, lat + 0.57, "80.0");
			cpgtext(lon + 0.1, lat + 0.47, "40.0");
			cpgtext(lon + 0.1, lat + 0.37, "25.0");
			cpgtext(lon + 0.1, lat + 0.27, "15.0");
			cpgtext(lon + 0.1, lat + 0.17, "8.0");
			cpgtext(lon + 0.1, lat + 0.07, "2.5");
			cpgtext(lon - 0.05, lat + 1.02, "Sa(gal)");

			for( i=0; i<10; i++) {
				cpgsci(COLOR_INDEX_BASE + i);
				cpgrect( lon, lon + 0.08, lat + i*0.1, lat + 0.08 + i*0.1);
			}

			break;
		case SHAKE_CWBPGV:
			cpgtext(lon + 0.13, lat - 0.03, "0.0");
			cpgtext(lon + 0.13, lat + 0.88, "75.0");
			cpgtext(lon + 0.13, lat + 0.75, "49.0");
			cpgtext(lon + 0.13, lat + 0.62, "17.0");
			cpgtext(lon + 0.13, lat + 0.49, "5.70");
			cpgtext(lon + 0.13, lat + 0.36, "1.90");
			cpgtext(lon + 0.13, lat + 0.23, "0.65");
			cpgtext(lon + 0.13, lat + 0.10, "0.22");
			cpgtext(lon - 0.05, lat + 1.05, "PGV(cm/s)");

			for( i=0; i<8; i++) {
				cpgsci(COLOR_INDEX_BASE + i);
				cpgrect(lon, lon + 0.1, lat + i*0.13, lat + 0.1 + i*0.13 );
			}
			break;
		case SHAKE_CWB2020:
			cpgtext(lon + 0.1, lat - 0.03, "0");
			cpgtext(lon + 0.1, lat + 0.87, "7");
			cpgtext(lon + 0.1, lat + 0.77, "6H");
			cpgtext(lon + 0.1, lat + 0.67, "6L");
			cpgtext(lon + 0.1, lat + 0.57, "5H");
			cpgtext(lon + 0.1, lat + 0.47, "5L");
			cpgtext(lon + 0.1, lat + 0.37, "4");
			cpgtext(lon + 0.1, lat + 0.27, "3");
			cpgtext(lon + 0.1, lat + 0.17, "2");
			cpgtext(lon + 0.1, lat + 0.07, "1");
			cpgtext(lon - 0.05, lat + 1.02, "");  /* Prof. Wu's decision */

			for( i=0; i<10; i++) {
				cpgsci(COLOR_INDEX_BASE + i);
				cpgrect(lon, lon + 0.08, lat + i*0.1, lat + 0.08 + i*0.1);
			}
			break;
		default:
			break;
	}
	cpgunsa();

	return;
}

/*
 *
 */
static void plot_gridline( const float minlon, const float maxlon, const float minlat, const float maxlat )
{
	float xline[2], yline[2];

	xline[0] = minlon;
	xline[1] = maxlon;
	yline[0] = (int)(maxlat/0.5) * 0.5;
	if ( fabs(yline[0] - maxlat) < FLT_EPSILON ) yline[0] -= 0.5;
	yline[1] = yline[0];

	do {
		cpgline(2, xline, yline);
		yline[0] -= 0.5;
		yline[1] -= 0.5;
	} while ( yline[0] > minlat );

	xline[0] = (int)(maxlon/0.5) * 0.5;
	if ( fabs(xline[0] - maxlon) < FLT_EPSILON ) xline[0] -= 0.5;
	xline[1] = xline[0];
	yline[0] = minlat;
	yline[1] = maxlat;

	do {
		cpgline(2, xline, yline);
		xline[0] -= 0.5;
		xline[1] -= 0.5;
	} while ( xline[0] > minlon );

	return;
}

/*
 *
 */
static void free_polyline( POLY_LINE *polyLine )
{
	POLY_LINE *plptr = NULL;
	POLY_LINE *next  = NULL;

	for ( plptr = polyLine; plptr != NULL; plptr = next ) {
		next = plptr->next;
		free(plptr->x);
		free(plptr->y);
		free(plptr);
	}

	return;
}
