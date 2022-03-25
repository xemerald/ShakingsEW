
# This is peak2trig's parameter file

# Basic Earthworm setup:
#
MyModuleId         MOD_PEAK2TRIG  # module id for this instance of template
InputRing          PEAK_RING      # shared memory ring for input peak value
OutputRing         TRIG_RING      # shared memory ring for output triglist;
                                  # if not define, will close this function
LogFile            1              # 0 to turn off disk log file; 1 to turn it on
                                  # to log to module log but not stderr/stdout
HeartBeatInterval  15             # seconds between heartbeats

UpdateInterval     0              # setting for automatical updating interval (seconds). If set this
								  # parameter larger than 0, the program will update the stations'
								  # list with this interval; or the program will ignore the new
								  # incoming trace.

# Trigger criteria information:
#
#
TriggerTimeInterval   1              #
RecordTypeToTrig      acc            #
TriggerStations       6              #
#
PeakThreshold         1.5            #
PeakDuration          6.0            #
ClusterDistance       60.0           #
ClusterTimeGap        5.0            #

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
GetEventsFrom  INST_WILDCARD      MOD_WILDCARD    TYPE_TRACEPEAK

# Local station list:
#
# The local list for stations that will receive. By the way, the priority of local list
# is higher than the one from remote data. And the layout should be like these example:
#
# Station  Station   Network   Location    Latitude      Longitude       Elevation(km)
# Station   TEST       TW         --       23.050514     121.215483      1.25             # example
#
#@stationlist_peak2trig
