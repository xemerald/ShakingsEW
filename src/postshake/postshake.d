
# This is postshake's parameter file

# Basic Earthworm setup:
#
MyModuleId          MOD_POSTSHAKE   # module id for this instance of shakemap
RingName            GMAP_RING       # shared memory ring for input/output
LogFile             1               # 0 to turn off disk log file; 1 to turn it on
                                    # to log to module log but not stderr/stdout
HeartBeatInterval   15              # seconds between heartbeats

QueueSize           100             # max messages in internal circular msg buffer

RemoveShakeMap      0               # 0 to keep those shakemap files; 1 to remove those
                                    # files after posted. Defaults to 0 if this is not setted

# Directory to create the report files:
#
ReportPath           /home/.../ew/run/shakemap

# List the message logos to grab from transport ring:
#
#              Installation       Module          Message Types
# 1st bit
GetEventsFrom  INST_WILDCARD    MOD_SHAKEMAP     TYPE_GRIDMAP
# 2nd bit
GetEventsFrom  INST_WILDCARD    MOD_SHAKEMAPV    TYPE_GRIDMAP

# The type of the shakemap which will be generated & its title:
# So far, we already have PAPGA, PAPGV, PACd, PASa, CWBPGA, CWBPGV &
# CWB2020 total 7 different types of shakemap.
#
# Something about "Input Message":
# The input message column link with "GetEventsFrom" above, it use the bit
# position as setting method: The first input message list above should be
# the first bit from right to left; therefore, the second input message should
# be the second bit and so on... By the way, the input value should be in
# decimal & the maximum bits is 8.
#
# Something about "Map Title" & "Map Caption":
# The maximum length of title & caption are 256 characters, and these strings are
# optional that means you can keep these columns empty.
#
#              Map Types       Map Title                      Input Message       Map Caption
PlotShakeMap   PAPGA       "Realtime PGA Shakemap"                 1                  ""
PlotShakeMap   PAPGV       "Realtime PGV Shakemap"                 2                  ""
PlotShakeMap   CWB2020     "Realtime New CWB Scale Shakemap"       3                  ""
# PlotShakeMap   PASA        "Realtime Sa Shakemap"                4             "Period: 1.0s"
# PlotShakeMap   PACD        "Realtime Cd Shakemap"                8                    ""
# PlotShakeMap   CWBPGA      "Realtime CWB PGA Shakemap"           1                    ""
# PlotShakeMap   CWBPGV      "Realtime CWB PGV Shakemap"           2                    ""

# The range of the shakemap:
#
#             MinLongitude  MaxLongitude  MinLatitude  MaxLatitude
MapRange          119.3        122.4         21.6         25.5

# Alarm issue related setup:
#
IssueInterval     30

# File define the target zone & city boundary in latitude & longtitude
#
NormalPolyLineFile     /home/.../ew/run/params/TWN_new_boundary.txt

# Optional file, it define the shoreline in latitude & longtitude which should be emphasised.
# If this option is commented out, the module will not emphasise the shoreline.
#
StrongPolyLineFile     /home/.../ew/run/params/TWN_map.txt

# Optional email program. It is imperative that the email program
# is capable of handling html email. postshake has been created
# specifically for sendmail. If this option is commented out, the
# module assumes that emails are not to be sent.
#
EmailProgram        /usr/sbin/sendmail

# Optional to set the prefix of the subject to customize the email. Defaults to EEW if this is not setted.
#
SubjectPrefix       "EEW"

# Setting for the email address of sender. Defaults to "EEW System <eews@eews.com>" if this is not setted.
#
EmailFromAddress    "EEW System <eews@eews.com>"

# List of email recipients:
#
EmailRecipient              test@gmail.com
EmailRecipientWithMinMag    test@gmail.com      5.0   # only send to this person if mag is >= 5.0

# Link URL for shakemap data which will be writen on email:
#
LinkURLPrefix          YOUR_PATH      # Defaults to "/" if this is not set.

# Post to the other place:
# This function is designed especially for executing external script.
# And it will be called like this: "script_name start_time end_time report_time max_magnitude trigstations
# result_filename_1 [result_filename_2]..."
# If you don't want to use it, please comment it out!
#
# PostScript            /home/.../ew/run/params/post_facebook.py
# PostScriptWithMinMag  /home/.../ew/run/params/post_facebook.py    5.0
