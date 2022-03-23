
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

# Switch of module operating type, you can switch between differential(diff) &
# integral(int):
#
OperationType      int

# Setting of high-pass filter applied after integral, it include order & corner of
# filter. The default value for these are 2 & 0.075 respectively. If this module is
# running under differential mode, you can just skip it.
#
HighPassOrder       2
HighPassCorner      0.075

# Settings for pre-processing of trace:
#
DriftCorrectThreshold   30        # seconds waiting for D.C.

# First two character of channel code for the trace after processing, if you don't want
# to change the code just comment it:
#
PostChannelCode     "HH"

# List the message logos to grab from transport ring
#              Installation       Module          Message Types
GetEventsFrom  INST_WILDCARD      MOD_WILDCARD    TYPE_TRACEBUF2
