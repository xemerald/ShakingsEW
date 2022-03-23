# coding:utf-8
# using python version 2
import os
import time
import datetime
import sys
import tweepy

##
api_key       = "YOUR_API_KEY"
api_secret    = "YOUR_API_SECRET"
bearer_token  = "YOUR_BEARER_TOKEN"
access_token  = "YOUR_ACCESS_TOKEN"
access_secret = "YOUR_ACCESS_SECRET"
TwitterAPI    = None

#
#
#
def main():
	global api_key
	global api_secret
	global access_token
	global access_secret
	global TwitterAPI

# Check the number of arguments
	argc = len(sys.argv)
	if ( argc < 7 ):
		print "Usage: python post_twitter.py start_time end_time report_time magnitude trig_stations filename_1 [filename_2]..."
		return
# Connect to the facebook api using the set token
	if TwitterAPI is None:
		TwitterAPI = get_twitter_api(api_key, api_secret, access_token, access_secret)
# Upload the picture to the Twitter
	media_ids = []
	for filename in sys.argv[6:]:
		media_ids.append(post_pic2tw(filename))
# Check if the first time of this event
	report_time = int(sys.argv[3])
	status_text = gen_status_text( gen_timestamp(sys.argv[2]), gen_report_str(report_time), sys.argv[1], sys.argv[5] )
	status = {}
	if ( report_time == 0 ):
		status = update_status2tw(status_text, media_ids)
	else:
		status = reply_status2tw(status_text, fetch_status_id_file(sys.argv[1]), media_ids)
# Remove or create the file for saving status id
	if ( report_time < 0 ):
		remove_status_id_file(sys.argv[1])
	else:
		gen_status_id_file(sys.argv[1], str(status.id))

	return

#
#
#
def update_status2tw(status, media_ids = []):
	global TwitterAPI
	return TwitterAPI.update_status(status = status, media_ids = media_ids)

#
#
#
def reply_status2tw(status, status_id, media_ids = []):
	global TwitterAPI
	return TwitterAPI.update_status(status = status, in_reply_to_status_id = status_id, media_ids = media_ids, auto_populate_reply_metadata = True)

#
#
#
def post_pic2tw(picfile):
	global TwitterAPI
	posted_media = TwitterAPI.media_upload(picfile)
	return posted_media.media_id

#
#
#
def get_twitter_api(c_key, c_secret, a_token, a_secret):
	twitter_auth = tweepy.OAuthHandler(c_key, c_secret)
	twitter_auth.set_access_token(a_token, a_secret)
	return tweepy.API(twitter_auth)

#
#
#
def gen_status_text(timestamp, report_str, evtid, trig_stations):
	result = ("Automatically Plotted Shakemaps:\n"
				"* " + timestamp + "\n"
				"* Evt. " + evtid + ", " + report_str + "\n"
				"* Total " + trig_stations + " triggered stations\n"
				"\nNote: These shakemaps are not yet confirmed, for reference only!\n"
				u"#地震 #台灣 #earthquake #Taiwan")
	return result

#
#
#
def gen_timestamp(endtime):
	result = "{}-{}-{} {}:{}:{} (UTC+08:00, Taipei)".format(str(endtime[:4]), str(endtime[4:6]), str(endtime[6:8]),
		str(endtime[8:10]), str(endtime[10:12]), str(endtime[12:14]))
	return result

#
#
#
def gen_report_str(report_time):
	result = ""
	if ( report_time == 0 ):
		result = "First Report."
	elif ( report_time < 0 ):
		result = "Final Report."
	else:
		_timestr = ""
		if ( report_time > 60 ):
			_timestr = str(report_time / 60) + "m" + str(report_time % 60) + "s"
		else:
			_timestr = str(report_time) + "s"
		result = _timestr + " after trig."
	return result

#
#
#
def gen_status_id_file(start_time, status_id):
	filename = start_time + "_twitter_sid"
	file     = open(filename, "w")
	file.write(status_id)
	file.close()
	return filename

#
#
#
def fetch_status_id_file(start_time):
	filename  = start_time + "_twitter_sid"
	file      = open(filename, "r")
	status_id = file.read()
	return status_id

#
#
#
def remove_status_id_file(start_time):
	filename  = start_time + "_twitter_sid"
	if os.path.exists(filename):
		os.remove(filename)
	else:
		print "File " + filename + " does not exist!"
	return filename

#
#
#
if __name__ == "__main__":
	main()
