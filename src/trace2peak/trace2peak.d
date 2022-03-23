
# This is trace2peak's parameter file

# Basic Earthworm setup:
#
MyModuleId         MOD_TRACE2PEAK # module id for this instance of template
InputRing          RWAVE_RING     # shared memory ring for input wave trace
OutputRing         PEAK_RING      # shared memory ring for output peak data;
                                  # if not define, will close this function
LogFile            1              # 0 to turn off disk log file; 1 to turn it on
                                  # to log to module log but not stderr/stdout
HeartBeatInterval  15             # seconds between heartbeats

# Settings for pre-processing of trace:
#
DriftCorrectThreshold   30        # seconds waiting for D.C.


# List the message logos to grab from transport ring
#              Installation       Module          Message Types
GetEventsFrom  INST_WILDCARD      MOD_WILDCARD    TYPE_TRACEBUF2
