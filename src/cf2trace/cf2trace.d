
# This is cf2trace's parameter file

# Basic Earthworm setup:
#
MyModuleId         MOD_CF2TRACE   # module id for this instance of template
InWaveRing         WAVE_RING      # shared memory ring for output wave trace
OutWaveRing        RWAVE_RING     # shared memory ring for output real wave trace
ListRing           LIST_RING      # shared memory ring for input station list;
                                  # if not define, will close this function
LogFile            1              # 0 to turn off disk log file; 1 to turn it on
                                  # to log to module log but not stderr/stdout
HeartBeatInterval  15             # seconds between heartbeats

# List the stations list to grab from MySQL server & filename in local
#              Station list      Channel list
GetListFrom   PalertList_Test    PalertChannels

# List the message logos to grab from transport ring
#              Installation       Module          Message Types
GetEventsFrom  INST_WILDCARD      MOD_WILDCARD    TYPE_TRACEBUF2
