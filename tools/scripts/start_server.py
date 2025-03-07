import os
import sys
import subprocess
import time
import argparse
import platform

def install_requirements():
    """Install the packages listed in requirements.txt."""
    requirements_path = "./tools/config/requirements.txt"
    try:
        subprocess.check_call(
            [sys.executable, "-m", "pip", "install", "-r", requirements_path],
            stdout=subprocess.DEVNULL    
        )
    except subprocess.CalledProcessError as e:
        print(f"Error installing requirements: {e}")
        sys.exit(1)

install_requirements()

import serial.tools.list_ports
import psutil

def list_serial_ports():
    """List available serial ports (cross-platform)."""
    ports = serial.tools.list_ports.comports()
    return ports

def get_current_directory():
    return os.getcwd()

def run_docker_command(is_flash=False):
    current_directory = get_current_directory()
    docker_command = [
        "docker", "run", "--rm", "-v", f"{current_directory}:/project", "-w", "/project",
        "-e", "HOME=/tmp", "-it", "--name", "UCF-Senior-Design", "espressif/idf"
    ]
    if is_flash:
        docker_command.extend(["idf.py", "--port", "rfc2217://host.docker.internal:4000?ign_set_control", "flash"])
    print(f"\nRunning Docker command:\n{' '.join(docker_command)}\n")
    time.sleep(1)
    subprocess.run(docker_command)

def run_rfc2217_server(serial_port):
    current_directory = os.getcwd()
    print(f"\nStarting RFC2217 server on {serial_port}...\n")
    
    if platform.system() == "Darwin":
        # macOS: Use nohup and disown to keep the process running after the script ends
        rfc_command = f"nohup {sys.executable} {current_directory}/esp_rfc2217_server.py -v -p 4000 {serial_port} & disown"
        try:
            subprocess.Popen(rfc_command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            time.sleep(1)
            print("RFC2217 server started successfully on macOS.\n")
        except Exception as e:
            print(f"Error starting RFC2217 server on macOS: {e}\n")
            sys.exit(1)
    
    elif platform.system() == "Windows":
        DETACHED_PROCESS = 0x00000008
        rfc_command = [sys.executable, "esp_rfc2217_server.py", "-v", "-p", "4000", serial_port]
        try:
            rfc_process = subprocess.Popen(rfc_command, creationflags=DETACHED_PROCESS, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            time.sleep(1)
            if rfc_process.poll() is None:
                print("RFC2217 server started successfully.\n")
            else:
                stdout, stderr = rfc_process.communicate()
                print("Failed to start RFC2217 server. Error output:\n")
                print(stderr.decode())
                sys.exit(1)
        except Exception as e:
            print(f"Error starting RFC2217 server: {e}\n")
            sys.exit(1)

def run_background_script():
    os_type = platform.system()
    if os_type == "Windows":
        try:
            ps_command = ["powershell", "-WindowStyle", "Hidden", "-Command", "python", "start_server.py"]
            subprocess.Popen(ps_command, creationflags=subprocess.CREATE_NO_WINDOW)
        except Exception as e:
            print(f"Error starting PowerShell script: {e}")
            sys.exit(1)
    else:
        try:
            # subprocess.Popen([sys.executable, "start_server.py"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            current_directory = get_current_directory()
            # Create the AppleScript command to open a new Terminal window and run the script
            mac_command = f"zsh -c 'nohup python {current_directory}/start_server.py & disown'"

            # Run the command to start the server in a new Terminal window
            subprocess.Popen(mac_command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, shell=True)
        except Exception as e:
            print(f"Error starting background script: {e}")
            sys.exit(1)

def check_and_terminate_port(port):
    for proc in psutil.process_iter(attrs=['pid', 'name']):
        try:
            for conn in proc.net_connections(kind='inet'):
                if conn.laddr.port == port:
                    print(f"Port {port} in use by process {proc.info['name']} (PID: {proc.info['pid']}). Terminating...")
                    proc.terminate()
                    proc.wait()
                    print("Terminated successfully.\n")
                    return True
        except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess, AttributeError):
            continue
    return False

def main():
    parser = argparse.ArgumentParser(description="Run Docker container and flash ESP32.")
    parser.add_argument('--flash', action='store_true', help="Flash the ESP32 after starting the server.")
    parser.add_argument('--start-container', action='store_true', help="Start the Docker container without flashing.")
    parser.add_argument('--non-interactive', action='store_true', help="Start the RFC2217 server in background.")
    args = parser.parse_args()
    
    if check_and_terminate_port(4000):
        print("\nPort 4000 was in use and has been terminated.\n")
    else:
        print("Port 4000 is free.\n")
    
    if args.non_interactive:
        run_background_script()
    
    ports = list_serial_ports()
    if not ports:
        print("No serial ports found.")
        if args.start_container:
            run_docker_command()
        else:
            print("Please connect the device and try again.")
            sys.exit(0)
    else:
        print("Available serial ports:")
        for port in ports:
            print(f" - {port.device}: {port.description}")
    
    os_type = platform.system()
    print(f"Your operating system is: {os_type}")
    match_found = False
    for port in ports:
        if os_type == "Windows" and "Silicon Labs CP210x USB to UART Bridge" in port.description:
            match_found = True
        elif os_type == "Darwin" and port.device.startswith("/dev/cu.SLAB_USBtoUART"):
            match_found = True
        elif os_type == "Linux" and port.device.startswith("/dev/ttyUSB"):
            match_found = True
        
        if match_found:
            print(f"Match found: {port.device} -> {port.description}")
            run_rfc2217_server(port.device)
            break
    
    if not match_found:
        if args.start_container:
            run_docker_command()
        else:
            print("No matching serial port found. Install the CP2102 driver, connect the device, and retry.")
            sys.exit(0)
    else:
        if args.flash:
            run_docker_command(is_flash=True)
        elif args.start_container:
            run_docker_command()

if __name__ == "__main__":
    main()
