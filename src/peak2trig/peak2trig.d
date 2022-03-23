
# This is peak2trig's parameter file

# Basic Earthworm setup:
#
MyModuleId         MOD_PEAK2TRIG  # module id for this instance of template
InputRing          PEAK_RING      # shared memory ring for input peak value
OutputRing         TRIG_RING      # shared memory ring for output triglist;
                                  # if not define, will close this function
ListRing           LIST_RING      # shared memory ring for input station list;
                                  # if not define, will close this function
LogFile            1              # 0 to turn off disk log file; 1 to turn it on
                                  # to log to module log but not stderr/stdout
HeartBeatInterval  15             # seconds between heartbeats

#
TriggerTimeInterval    1              #
RecordTypeToTrig       acc            #
TriggerStations        6              #

PeakThreshold      1.5            #
PeakDuration       6.0            #
ClusterDistance    60.0           #
ClusterTimeGap     5.0            #

# List the stations list to grab from MySQL server & filename in local
#              Station list      Channel list
GetListFrom   PalertList_Test    PalertChannels

# List the message logos to grab from transport ring
#              Installation       Module          Message Types
GetEventsFrom  INST_WILDCARD      MOD_WILDCARD    TYPE_TRACEPEAK
