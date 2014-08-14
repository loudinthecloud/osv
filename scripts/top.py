#!/usr/bin/env python3
# Connect to a running OSv guest through the HTTP API and periodically display
# the list of threads getting most CPU time, similarly to the Linux top(1)
# command.
#
# The host:port to connect to can be specified on the command line, or
# defaults to localhost:8000 (which is where "run.py --api" redirects the
# guest's API port). If only a hostname is specified, port 8000 is used by
# default.

import requests
import argparse
import json
import time
import sys
import collections

from osv.client import Client

try:
    import curses
    curses.setupterm()
    clear = curses.tigetstr('clear').decode()
except:
    clear = '\033[H\033[2J'

parser = argparse.ArgumentParser(description="""
    Connects to a running OSv guest through the HTTP API and periodically displays
    the list of threads getting most CPU time, similarly to the Linux top(1)
    command.""")
Client.add_arguments(parser)

args = parser.parse_args()
client = Client(args)

url = client.get_url() + "/os/threads"
ssl_kwargs = client.get_request_kwargs()

period = 2.0  # How many seconds between refreshes

prevtime = collections.defaultdict(int)
cpu = dict()
name = dict()
timems = 0
while True:
    start_refresh = time.time()
    result = requests.get(url, **ssl_kwargs).json()
    print(clear, end='')
    newtimems = result['time_ms']

    print("%d threads " % (len(result['list'])), end='')
    diff = dict()
    idles = dict()
    for thread in result['list']:
        id = thread['id']
        cpu[id] = thread['cpu']
        cpums = thread['cpu_ms']
        if timems:
            diff[id] = cpums - prevtime[id]
        prevtime[id] = cpums
        name[id] = thread['name']
        # Display per-cpu idle threads differently from normal threads
        if thread['name'].startswith('idle') and timems:
            idles[thread['name']] = diff[id]
            del diff[id]

    if idles:
        print("on %d CPUs; idle: " % (len(idles)), end='')
        total = 0.0
        for n in sorted(idles):
            percent = 100.0*idles[n]/(newtimems - timems)
            total += percent
            print ("%3.0f%% "%percent, end='')
        if len(idles) > 1:
            print(" (total %3.0f%%)" % total)
    print("")

    print("%5s %3s  %s %7s %s" % ("ID", "CPU", "%CPU", "TIME", "NAME"))
    for id in sorted(diff, key=lambda x : (diff[x], prevtime[x]), reverse=True)[:20]:
        percent = 100.0*diff[id]/(newtimems - timems)
        print("%5d %3d %5.1f %7.2f %s" % (id, cpu[id], percent, prevtime[id]/1000.0, name[id]))
    timems = newtimems
    time.sleep(max(0, period - (time.time() - start_refresh)))
