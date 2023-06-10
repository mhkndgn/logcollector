import fnmatch
import os
import datetime
from datetime import datetime as dt
from datetime import date
import subprocess

def cron_job(s, temp_folder_path, lifetime):
    datetime_now = datetime.datetime.now()
    #print(datetime_now.day)
    #print(datetime_now.month)
    #print(datetime_now.year)
    _temp_folder_path = temp_folder_path.strip()
    for root, dirs, files in os.walk(_temp_folder_path):
        for filename in files:
            stat = os.stat(root + "/" + filename)
            creation_date = dt.fromtimestamp(stat.st_ctime)
            delta = date(datetime_now.year, datetime_now.month, datetime_now.day) - \
                    date(creation_date.year, creation_date.month, creation_date.day)

            if delta.days > int(lifetime):
                print(root + "/" + filename + " is older than " + str(lifetime) + " days. Deleting...")
                os.remove(root + "/" + filename)

    s.enter(60 * 60, 1, cron_job, (s, _temp_folder_path, lifetime))

def mv_and_compress_files(s, arhive_folder_path, json_folder_path, lifetime):
    datetime_now = datetime.datetime.now()
    _json_folder_path = json_folder_path.strip()
    arhive_folder_path = arhive_folder_path.strip()
    for root, dirs, files in os.walk(_json_folder_path):
        for filename in files:
            stat = os.stat(root + "/" + filename)
            creation_date = dt.fromtimestamp(stat.st_ctime)
            delta = date(datetime_now.year, datetime_now.month, datetime_now.day) - \
                    date(creation_date.year, creation_date.month, creation_date.day)

            if delta.days > int(lifetime):
                print(root + "/" + filename + " is older than " + str(lifetime) + " days. Moving and compressing...")
                new_filename = filename.replace(":",".") 
                os.rename(root + "/" + filename, root + "/" + new_filename)
                subprocess.call(['tar', '-czf', root + "/"+ new_filename + ".tar.gz", root + "/" + new_filename])
                os.makedirs(arhive_folder_path + "/"+ str(creation_date.year) + "/" + str(creation_date.month) + "/" + str(creation_date.day) + "/",mode=0o777,exist_ok=True)
                subprocess.call(['mv', root + "/"+ new_filename + ".tar.gz", arhive_folder_path + "/"+ str(creation_date.year) + "/" + str(creation_date.month) + "/" + str(creation_date.day) + "/"])
                os.remove(root + "/" + new_filename)

    s.enter(60 * 60, 1, mv_and_compress_files, (s, arhive_folder_path, _json_folder_path, lifetime))
