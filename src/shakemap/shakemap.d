
# This is shakemap's parameter file

#  Basic Earthworm setup:
#
MyModuleId          MOD_SHAKEMAP    # module id for this instance of shakemap
PeakRing            PEAK_RING       # shared memory ring for input peak value message
TrigRing            TRIG_RING       # shared memory ring for input trigger list
OutputRing          GMAP_RING       # shared memory ring for output grid map message;
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

# MySQL server information:
#
# If you setup the follow parameter especially SQLHost, this program will fetch
# list from MySQL server or you can just comment all of them, then it will turn
# off this function.
#
SQLHost         127.0.0.1         # The maximum length is 36 words
SQLPort         3306              # Port number between 1 to 65536
SQLDatabase     EEW	              # The maximum length is 36 words

# Login information example
#
# SQLUser       test
# SQLPassword   123456
#@LoginInfo_sql                    # Please keep the security of the SQL login information

# List the stations lists that will grab from MySQL server
#
# Even when you using MySQL server to fetch station information.
#
SQLStationTable    PalertList
#SQLStationTable    SecondList
#SQLStationTable    ThirdList

# List the message logos to grab from transport ring
#              Installation       Module          Message Types
GetEventsFrom  INST_WILDCARD    MOD_TRACE2PEAK   TYPE_TRACEPEAK
GetEventsFrom  INST_WILDCARD    MOD_PEAK2TRIG    TYPE_TRIGLIST

# Local station list:
#
# The local list for stations that will receive. By the way, the priority of local list
# is higher than the one from remote data. And the layout should be like these example:
#
# Station  Station   Network   Location    Latitude      Longitude       Elevation(km)
# Station   TEST       TW         --       23.050514     121.215483      1.25             # example
#
#@stationlist_shakemap
