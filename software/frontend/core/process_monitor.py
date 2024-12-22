# process_monitor.py
import psutil
import json
def get_running_processes():
    processes = []
    for proc in psutil.process_iter(['name']):
        try:
            if proc.info['name']:
                processes.append(proc.info['name'])
        except psutil.NoSuchProcess:
            pass
    return processes

def generate_background_process_list():
    processes = set()  # Use a set to automatically handle duplicates
    
    for proc in psutil.process_iter(['name']):
        try:
            if proc.info['name']:
                processes.add(proc.info['name'])  # Add to the set, duplicates will be ignored
        except psutil.NoSuchProcess:
            pass
    
    # Save the unique background processes to a JSON file
    with open("settings/backgroundApps.json", "w") as file:
        json.dump({"backgroundApps": list(processes)}, file, indent=4)  # Convert set to list

