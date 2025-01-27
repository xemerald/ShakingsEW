# This is dif2trace's parameter file

# Basic Earthworm setup:
#
MyModuleId         MOD_DIF2TRACE  # module id for this instance of template
InWaveRing         RWAVE_RING     # shared memory ring for output wave trace
OutWaveRing        XWAVE_RING     # shared memory ring for output real wave trace
                                  # if not define, will close this function
LogFile            1              # 0 to turn off disk log file; 1 to turn it on
                                  # to log to module log but not stderr/stdout
HeartBeatInterval  15             # seconds between heartbeats

# Switch of module operating type, you can switch in differential(diff), double differential(ddiff),
# integral(int) & double integral(dint):
#
OperationType      int

# Setting of high-pass filter applied after integral, it include order & corner of
# filter. The default value for these are 2 & 0.075 respectively. If this module is
# running under differential(diff & ddiff) mode, you can just skip it.
#
HighPassOrder       2
HighPassCorner      0.075

# Settings for pre-processing of trace:
#
DriftCorrectThreshold   30        # seconds waiting for D.C.

# List the message logos to grab from transport ring
#              Installation       Module          Message Types
GetEventsFrom  INST_WILDCARD      MOD_WILDCARD    TYPE_TRACEBUF2

# List of station/channel/network/loc codes to process (Optional):
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
#                 Origin SCNL      Map to SCNL
#Block_SCNL       BOZ LHZ US *                    # Block this specific channel
#Allow_SCNL       JMP ASZ NC 01                   # Allow this specific channel
#Allow_SCNL       JPS *   NC *                    # Allow all components of JPS NC
#Allow_SCNL_Remap JGR VHZ NC --    *   EHZ * *    # Change component code only
#Allow_SCNL_Remap CAL *   NC *     ALM *   * *    # Allow all component of CAL, but change the site code to ALM
