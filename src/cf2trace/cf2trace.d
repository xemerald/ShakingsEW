
# This is cf2trace's parameter file

# Basic Earthworm setup:
#
MyModuleId         MOD_CF2TRACE   # module id for this instance of template
InWaveRing         WAVE_RING      # shared memory ring for output wave trace
OutWaveRing        RWAVE_RING     # shared memory ring for output real wave trace
                                  # if not define, will close this function
LogFile            1              # 0 to turn off disk log file; 1 to turn it on
                                  # to log to module log but not stderr/stdout
HeartBeatInterval  15             # seconds between heartbeats

UpdateInterval     0              # setting for automatical updating interval (seconds). If set this
                                  # parameter larger than 0, the program will update the channels'
                                  # list with this interval; or the program will ignore the new
                                  # incoming trace.
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

# List the channels lists that will grab from MySQL server
#
# Even when you using MySQL server to fetch channel information.
#
SQLChannelTable    PalertChannelList
#SQLChannelTable    SecondChannelList
#SQLChannelTable    ThirdChannelList

# List the message logos to grab from transport ring
#              Installation       Module          Message Types
GetEventsFrom  INST_WILDCARD      MOD_WILDCARD    TYPE_TRACEBUF2

# Local channel list:
#
# The local list for channels that will receive. By the way, the priority of local list
# is higher than the one from remote data. And the layout should be like these example:
#
# SCNL     Station   Network   Location   Channel     Record_Type      Conversion_Factor(gal/count)
# SCNL      TEST      TW         --        HLZ          acc               0.059814453           # first example
# SCNL      TEST      TW         --        HLN          acc               0.059814453           # second example
# SCNL      TEST      TW         --        HLE          acc               0.059814453           # third example
#
#@channellist_cf2tra
