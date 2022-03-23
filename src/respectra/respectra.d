
# This is respectra's parameter file

# Basic Earthworm setup:
#
MyModuleId         MOD_RESPECTRA  # module id for this instance of template
InWaveRing         RWAVE_RING     # shared memory ring for output wave trace
OutWaveRing        XWAVE_RING     # shared memory ring for output real wave trace
                                  # if not define, will close this function
LogFile            1              # 0 to turn off disk log file; 1 to turn it on
                                  # to log to module log but not stderr/stdout
HeartBeatInterval  15             # seconds between heartbeats

# Switch of the output value type, you can switch among "sa", "sv" and "sd":
#
OutputType         Sa

# Settings for computation of spectra, include critical damping ratio in fractions &
# the specified natural period:
#
DampingRatio       0.05
NaturalPeriod      0.3

# Settings for pre-processing of trace:
#
DriftCorrectThreshold   30        # seconds waiting for D.C.

# First two character of channel code for the trace after processing, if you don't want
# to change the code just comment it:
#
PostChannelCode     "RA"

# List the message logos to grab from transport ring
#              Installation       Module          Message Types
GetEventsFrom  INST_WILDCARD      MOD_WILDCARD    TYPE_TRACEBUF2
