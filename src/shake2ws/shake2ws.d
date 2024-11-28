
# This is shake2ws's parameter file

# Basic Earthworm setup:
#
MyModuleId         MOD_SHAKE2WS   # module id for this instance of template
InputRing          PEAK_RING      # shared memory ring for input wave trace
LogFile            1              # 0 to turn off disk log file; 1 to turn it on
                                  # to log to module log but not stderr/stdout
HeartBeatInterval  15             # seconds between heartbeats

# Settings for Websocket server:
#
WSPort             9999           #

# List the message logos to grab from transport ring
#              Installation       Module          Message Types
GetEventsFrom  INST_WILDCARD      MOD_WILDCARD    TYPE_TRACEPEAK


# The type of the peak value which will be setted:
#
# Something about "Table Key Prefix":
# The Hash table name would be like "XXX:<UNIX_TIMESTAMP>", where XXX is
# the prefix you setted here. And max length of prefix is 16 (include NULL termination).
#
# List the peak value type you want to set in the redis server.
#                  Index#    Value Types    Intensity Types
SetPeakValueType     0          acc             CWBPGA
SetPeakValueType     1          vel             CWBPGV
SetPeakValueType     2          dis             PACD
SetPeakValueType     3          sa              CWBPGA
SetPeakValueType     4          sa              CWBPGA

# List of generating intensity index (Under construction):
#
# Something about "Input Peak Value":
# The input value column link with "SetPeakValueType" above, it use the bit
# position as setting method: The first input value list above should be
# the first bit from right to left; therefore, the second input message should
# be the second bit and so on... By the way, the input value should be in
# decimal & the maximum bit position is 8.
#
# List the Intensity type to generate from those peak values.
#                   Intensity Types    Input Peak Value
GenIntensityType        CWBPGA                1
GenIntensityType        CWBPGV                2
GenIntensityType        CWB2020               3
GenIntensityType        CWBPGA                8
GenIntensityType        CWBPGA               16

# List of station/channel/network/loc codes to process and its set value index (link to SetPeakValueType):
#
# Use any combination of Allow_SCNL (to process data as-is) and
# Allow_SCNL_Remap (to change the SCNL on the fly) commands.
#
# Use * as a wildcard for any field. A wildcard in the
# "Map to" fields of Allow_SCNL_Remap means that field will
# not be renamed.
#
# Use the Block_SCNL command (works with both wildcards
# and not, but only makes sense to use with wildcards) to block
# any specific channels that you don't want to process.
#
# Note, the Block commands must precede any wildcard commands for
# the blocking to occur.
#
#                 Origin SCNL      Map to SCNL      Set Value Index
#Block_SCNL       BOZ LHZ US *                                            # Block this specific channel
#Allow_SCNL       JMP ASZ NC 01                          0                # Allow this specific channel
                                                                          # and it belongs to PGA
#Allow_SCNL       JPS *   NC *                           1                # Allow all components of JPS NC
                                                                          # and they all belong to PGV
#Allow_SCNL_Remap JGR VHZ NC --    *   EHZ * *           1                # Change component code only
#Allow_SCNL_Remap CAL *   NC *     ALM *   * *           2                # Allow all component of CAL, but change the site code to ALM
