# process_monitor.py
import psutil

def get_running_processes():
    processes = []
    for proc in psutil.process_iter(['name']):
        try:
            if proc.info['name']:
                processes.append(proc.info['name'])
        except psutil.NoSuchProcess:
            pass
    return processes
