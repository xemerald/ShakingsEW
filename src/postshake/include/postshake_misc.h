#pragma once

/* Function prototype */
char *GenSmapFilename( const PLOTSMAP *, char *, size_t );
int GetTrigStations( const PLOTSMAP * );

GRIDMAP_HEADER *GetRefGridmap( const PLOTSMAP * );
