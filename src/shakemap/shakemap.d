
# This is shakemap's parameter file

#  Basic Earthworm setup:
#
MyModuleId          MOD_SHAKEMAP    # module id for this instance of shakemap
PeakRing            PEAK_RING       # shared memory ring for input peak value message
TrigRing            TRIG_RING       # shared memory ring for input trigger list
OutputRing          GMAP_RING       # shared memory ring for output grid map message;
                                    # if not define, will close this function
ListRing            LIST_RING       # shared memory ring for input station list;
                                    # if not define, will close this function
LogFile             1               # 0 to turn off disk log file; 1 to turn it on
                                    # to log to module log but not stderr/stdout
HeartBeatInterval   15              # seconds between heartbeats

QueueSize           10              # max messages in internal circular msg buffer

#  Algorithm related parameters:
#
TriggerAlgType          1           # refer to the header file triglist.h;
                                    # 0: hypo, 1: peak cluster;
                                    # in fact you can use the message logo to restrict the trigger message,
                                    # so this parameter is just for double check.
PeakValueType           acc         # peak grid value type, include dis, vel and acc
TriggerDuration         30          # the duration time of each trigger list in second
InterpolateDistance     30.0        # the maximum distance when doing interpolation for each grid

#  Input/Output related parameters:
#
ReportPath      /home/.../ew/run/shakemap/             # directory to create the report files
MapBoundFile    /home/.../ew/run/params/taiwan.txt     # file define the target zone boundary in latitude & longtitude

# List the stations list to grab from MySQL server & filename in local
#              Station list      Channel list
GetListFrom   PalertList_Test    PalertChannels

# List the message logos to grab from transport ring
#              Installation       Module          Message Types
GetEventsFrom  INST_WILDCARD    MOD_TRACE2PEAK   TYPE_TRACEPEAK
GetEventsFrom  INST_WILDCARD    MOD_PEAK2TRIG    TYPE_TRIGLIST
