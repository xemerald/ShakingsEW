
# This is shake2redis's parameter file

# Basic Earthworm setup:
#
MyModuleId         MOD_SHAKE2REDIS # module id for this instance of template
InputRing          PEAK_RING       # shared memory ring for input wave trace
LogFile            1               # 0 to turn off disk log file; 1 to turn it on
                                   # to log to module log but not stderr/stdout
HeartBeatInterval  15              # seconds between heartbeats
QueueSize          1024            # max messages in internal circular msg buffer

# Settings for Redis server:
#
RedisHost             127.0.0.1    # Hostname of the Redis server, maximum length is 256
RedisPort             6379         # Connecting port of the Redis server, default is 6379
#RedisPassword         yourPassword # Optional argument for authentication of Redis server.
ExpireTime            86400        # Expired time of record in seconds

# List the message logos to grab from transport ring
#              Installation       Module          Message Types
GetEventsFrom  INST_WILDCARD      MOD_WILDCARD    TYPE_TRACEPEAK

# The type of the peak value which will be grabbed:
#
# Something about "Source Module":
# The Source Module column use the module id setted inside the earthworm.d
# You can directly use the wildcard if thera is only one module generate the type
# of value; Otherwise, you should designate the module id for distinguishing.
#
# List the peak value type to grab from transport ring.
#                  Value Types    Key Prefix      Source Module
GetPeakValueType      acc           PGA           MOD_WILDCARD
GetPeakValueType      vel           PGV           MOD_WILDCARD
GetPeakValueType      dis           PGD           MOD_WILDCARD
GetPeakValueType      sa            SAL           MOD_WILDCARD
GetPeakValueType      sa            SAS           MOD_RESPECTRA

#
#
# Something about "Input Value":
# The input value column link with "GetPeakValueType" above, it use the bit
# position as setting method: The first input value list above should be
# the first bit from right to left; therefore, the second input message should
# be the second bit and so on... By the way, the input value should be in
# decimal & the maximum bit position is 8.
#
# List the Intensity type to generate from those peak values.
#                  Intensity Types       Input Value
GenIntensityType        CWBPGA                1
GenIntensityType        CWBPGV                2
GenIntensityType        CWB2020               3
GenIntensityType        CWBPGA                8
GenIntensityType        CWBPGA               16
