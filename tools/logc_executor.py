#!/usr/bin/env python3
import subprocess
import os
import re
import sys
from os import path
import sched, time
import signal
from logc_cronjobs import cron_job, mv_and_compress_files
import _thread


def set_process(p):
    global g_ps
    g_ps = p

pid_of_collector = 0

def get_process():
    return g_ps


def signal_handler(sig, frame):
    print('You signalled the process. Agent process will be closed, but OS will be wait for app to free its rings! Good bye!')
    kill_command = "pkill -SIGINT logcoll                               b bbector"
    prss = subprocess.Popen(kill_command, shell=True, stdout=subprocess.PIPE)
    sys.exit(0)


def hex_to_bits(hex_data):
    my_hexdata = hex_data
    scale = 16
    num_of_bits = 8
    index_arr = bin(int(my_hexdata, scale))[2:].zfill(num_of_bits)[::-1]
    cores = ""
    for i in range(len(index_arr)):
        if index_arr[i] == "1":
            cores += str(i) + ","
    cores = cores[:-1]
    return cores


def process_exists(proc_name):
    ps = subprocess.Popen("ps ax -o pid= -o args= ", shell=True, stdout=subprocess.PIPE)
    ps_pid = ps.pid
    output = ps.stdout.read().decode("utf-8")
    ps.stdout.close()
    ps.wait()

    for line in str(output).split("\n"):
        res = re.findall("(\d+) (.*)", line)
        if res:
            pid = int(res[0][0])
            p_name = res[0][1]
            if proc_name in res[0][1] and pid != os.getpid() and pid != ps_pid and 'python' not in p_name:
                #print('->>>>>>>' + p_name + " : " + proc_name + " : " + res[0][0] + " : " + res[0][1])
                return True, pid
    return False


def check_process(s, _proc_name, _config_file_path, _receiver_core_ids, _master_core_ids, _core_ids, _key, _log_file_path, _log_level):
    time.sleep(3)
    if not process_exists(_proc_name):
        _thread.start_new_thread(start_logcollector_app, (_proc_name, _config_file_path, _receiver_core_ids, _master_core_ids, _core_ids, _key, _log_file_path, _log_level))
    s.enter(3, 1, check_process, (s, _proc_name, _config_file_path, _receiver_core_ids, _master_core_ids, _core_ids, _key, _log_file_path, _log_level,))


def start_logcollector_app(executable_file_path, _config_file_path, _receiver_core_ids, _master_core_ids, _core_ids, _key, _log_file_path, _log_level):
    if _log_file_path is not None:
        start_command = executable_file_path + " -f " + _config_file_path + " -s " + _receiver_core_ids + " -m " + _master_core_ids + " -c " + _core_ids + \
                        " -k " + _key + " -d " + _log_file_path + " -l " + _log_level
    else:
        start_command = executable_file_path + " -f " + _config_file_path + " -s " + _receiver_core_ids + " -m " + _master_core_ids + " -c " + _core_ids + \
                        " -k " + _key

    ps = subprocess.Popen(start_command, shell=True, stdout=subprocess.PIPE)
    print("start command : " + start_command + " and pid: " + str(ps.pid))
    id_of_collector = ps.pid
    for line in ps.stdout:
        line = line.rstrip()
        print(line.decode("utf-8"), end="\r\n", flush=True)


def validate_lifetime(lifetime):
    if lifetime[-1] == "D":
        lifetime = lifetime[:-1]
        return lifetime
    elif lifetime[-1] == "W":
        lifetime = lifetime[:-1]
        return lifetime * 7
    elif lifetime[-1] == "M":
        lifetime = lifetime[:-1]
        return lifetime * 30
    else:
        print("Lifetime is not valid! Please use xD (Day) or xW (Week) xM (Month)")
        exit(0)


def read_config_file_temp_folder(config_file_pth):
    with open(config_file_pth) as fp:
        line = fp.readline()
        _temp_file_path = ""
        while line:
            stripped = line.strip("\n")
            if stripped.find("tmp_folder:") > -1:
                stripped = stripped.split(":")
                _temp_file_path = stripped[1]
            line = fp.readline()

    if _temp_file_path != "" :
        return _temp_file_path
    else:
        print("Temp folder path is wrong! Please check!")
        exit(0)

def read_config_file_archive_folder(config_file_pth):
    with open(config_file_pth) as fp:
        line = fp.readline()
        _archive_file_path = ""
        _json_file_path = ""
        while line:
            stripped = line.strip("\n")
            if stripped.find("archive_folder_path:") > -1:
                stripped = stripped.split(":")
                _archive_file_path = stripped[1]
            line = fp.readline()

    with open(config_file_pth) as fp:
        line = fp.readline()
        _json_file_path = ""
        while line:
            stripped = line.strip("\n")
            if stripped.find("json_folder:") > -1:
                stripped = stripped.split(":")
                _json_file_path = stripped[1]
            line = fp.readline()

    if _archive_file_path != "" and _json_file_path != "":
        return _archive_file_path, _json_file_path
    else:
        print("Archive folder path is wrong! Please check!")
        exit(0)

def kill_old_process():
    kill_command = "pkill -SIGINT logcollector"
    prss = subprocess.Popen(kill_command, shell=True, stdout=subprocess.PIPE)

def get_logs_of_process(s, _ps):
    lines_iterator = iter(_ps.stdout.readline, b"")
    for line in lines_iterator:
        nline = line.rstrip()
        print(nline.decode("utf-8"), end="\r\n", flush=True)  # yield line
    s.enter(1, 1, get_logs_of_process, (s, get_process()))

def create_log_file(path):
    if not path == "":
        subprocess.call(['touch', path])
        return path
    else:
        subprocess.call(['touch', "/var/log/logcollector.log"])
        return "/var/log/logcollector.log"



def print_usage():
    print("\"sudo python3 logc_executor.py\" \"-r\" <executable path> \"-f\" <config path> "
          "\"-m\" <<hex ie 0xfc or a,b,c core ids> \"-s\" <<hex ie 0xfc or a,b,c core ids>"
          " \"-c\" <hex ie 0xfc or a,b,c core ids> \"-lt\" xD(ay) or xW(eek) or xM(onth) \"-alt\" xD(ay) or xW(eek) or xM(onth)\"-key\" <key>"
          " \"-d\"  <log_file_path> \"-l\" <log level : 1 or 2>")

    print("Example usage:\n\t$sudo python3 logc_executor.py -r ./logcollector -f config.conf -s 0x2 -m 0x1 -c 0x2 -lt 1D -alt 2D "
          "-key 545055525903075400565D575400025F5E56000156020D00050602015A00505751555F06 -d <log_file_path> (default /var/log/logcollector.log)"
          " -l <log_level> (1 or 2)")
    exit(0)

def clear_cache(s):
    clear_command = "echo 3 > /proc/sys/vm/drop_caches"
    ps = subprocess.Popen(clear_command, shell=True, stdout=subprocess.PIPE)
    s.enter(30, 1, clear_cache, (s,))


if __name__ == '__main__':

    if os.getuid() != 0:
        print("You must be super user!")
        exit(0)
    #print('Number of arguments:', len(sys.argv), 'arguments.')
    #print('Argument List:', str(sys.argv))

    if len(sys.argv) < 17:
        print_usage()

    if (    not "-r"    == sys.argv[1]   or
            not "-f"    == sys.argv[3]   or
            not "-s"    == sys.argv[5]   or
            not "-m"    == sys.argv[7]   or
            not "-c"    == sys.argv[9]   or
            not "-lt"   == sys.argv[11]   or
            not "-alt"  == sys.argv[13]  or
            not "-key"  == sys.argv[15]):
        print_usage()

    log_level = -1
    log_file_path = None

    if len(sys.argv) > 17 and "-d" == sys.argv[17]:
        if sys.argv[19] == "-l":
            log_level = sys.argv[20]
            log_file_path = create_log_file("")
        else:
            log_level = sys.argv[20]
            log_file_path = create_log_file(sys.argv[18])

    if (int(log_level) > 2 or int(log_level) < 1) and int(log_level) != -1:
        print_usage()

    proc_name = sys.argv[2]

    receiver_core_ids = sys.argv[6]
    if receiver_core_ids.find("x") > 0:
        receiver_core_ids = hex_to_bits(receiver_core_ids)

    master_core_ids = sys.argv[8]
    if master_core_ids.find("x") > 0:
        master_core_ids = hex_to_bits(master_core_ids)

    core_ids = sys.argv[10]
    if core_ids.find("x") > 0:
        core_ids = hex_to_bits(core_ids)

    temp_file_lifetime = validate_lifetime(sys.argv[12])
    archive_lifetime = validate_lifetime(sys.argv[14])
    key = sys.argv[16]

    config_file_path = sys.argv[4]
    temp_folder_path = ""
    archive_folder_path = ""
    json_folder_path = ""
    if not path.exists(config_file_path):
        print("Config file not found in path: " + config_file_path)
        exit(0)
    else:
        temp_folder_path = read_config_file_temp_folder(config_file_path)
        archive_folder_path, json_folder_path = read_config_file_archive_folder(config_file_path)

    if process_exists(proc_name):
        answer = input("Logcollector process is running already. Would you like to restart?: (y/N): ")
        if answer == "y":
            kill_old_process()
            _thread.start_new_thread(start_logcollector_app, (proc_name, config_file_path, receiver_core_ids, master_core_ids, core_ids, key, log_file_path, log_level))
        elif answer == "N":
            print("answer is N")
            sys.exit(0)
        else:
            while answer != "y" and answer != "N":
                answer = input("Please type \"y\" or \"N\": ")
                if answer == "y":
                    kill_old_process()
                    _thread.start_new_thread(start_logcollector_app, (proc_name, config_file_path, receiver_core_ids, master_core_ids, core_ids, key, log_file_path, log_level))
                    break
                elif answer == "N":
                    print("answer is N")
                    sys.exit(0)
            else:
                _thread.start_new_thread(start_logcollector_app, (proc_name, config_file_path, receiver_core_ids, master_core_ids, core_ids, key, log_file_path, log_level))

    signal.signal(signal.SIGINT, signal_handler)

    s = sched.scheduler(time.time, time.sleep)
    s.enter(1, 1, check_process, (s, proc_name, config_file_path, receiver_core_ids, master_core_ids, core_ids, key, log_file_path, log_level, ))
    s.enter(1, 1, clear_cache, (s,))
    s.enter(1, 1, cron_job, (s, temp_folder_path, temp_file_lifetime,))
    s.enter(1, 1, mv_and_compress_files, (s, archive_folder_path, json_folder_path, archive_lifetime,))
    s.run()
