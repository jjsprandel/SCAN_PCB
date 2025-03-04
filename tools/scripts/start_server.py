import os
import sys
import subprocess
import time
import argparse

requirements_path = "./tools/config/requirements.txt"

def install_requirements():
    """Install the packages listed in requirements.txt."""
    try:
        subprocess.check_call([sys.executable, "-m", "pip", "install", "-r", requirements_path])
    except subprocess.CalledProcessError as e:
        print(f"Error installing requirements: {e}")
        sys.exit(1)

install_requirements()

import serial.tools.list_ports
import psutil  # Import psutil to check for processes using a port

def list_serial_ports():
    ports = serial.tools.list_ports.comports()
    return ports

def get_current_directory():
    return os.getcwd()

def run_docker_command(is_flash=False):
    current_directory = get_current_directory()

    # Base Docker run command
    docker_command = [
       "docker", "run", "--rm", "--build", "-v", f"{current_directory}:/project", "-w", "/project", 
        "-e", "HOME=/tmp", "-it", "--name", "UCF-Senior-Design", "--file", "./Dockerfile", "espressif/idf"
    ]
    
    if is_flash:
        docker_command.append("idf.py")
        docker_command.extend(["--port", "rfc2217://host.docker.internal:4000?ign_set_control", "flash"])

    # Print the docker command that will be executed
    print(f"\nRunning Docker command:\n{' '.join(docker_command)}\n")

    # Wait for 1 second before executing the command
    time.sleep(1)

    # Run Docker command in the same terminal
    subprocess.run(docker_command)

def run_rfc2217_server(serial_port):
    print(f"\nStarting RFC2217 server on {serial_port}...\n")
    rfc_command = ["python", "esp_rfc2217_server.py", "-v", "-p", "4000", serial_port]
    
    # Run RFC2217 server in the background and capture output
    try:
        rfc_process = subprocess.Popen(rfc_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        time.sleep(1)  # Allow time for the server to start

        # Check if the server started successfully
        if rfc_process.poll() is None:
            print("RFC2217 server started successfully.\n")
        else:
            # Capture and display error output
            stdout, stderr = rfc_process.communicate()
            print("Failed to start RFC2217 server. Error output:\n")
            print(stderr.decode())
            sys.exit(1)  # Exit if the server couldn't start
    except Exception as e:
        print(f"Error starting RFC2217 server: {e}\n")
        sys.exit(1)

def run_power_shell_script():
    # This will run the PowerShell command without showing the window
    try:
        ps_command = [
            "powershell", "-WindowStyle", "Hidden", "-Command", 
            "python", "start_server.py"
        ]
        subprocess.Popen(ps_command, creationflags=subprocess.CREATE_NO_WINDOW)
    except Exception as e:
        print(f"Error starting PowerShell script: {e}")
        sys.exit(1)

def check_and_terminate_port(port):
    """Check if a port is in use and terminate the process using that port."""
    for proc in psutil.process_iter(attrs=['pid', 'name']):
        try:
            # Attempt to check for connections and see if the port is being used
            for conn in proc.net_connections(kind='inet'):
                if conn.laddr.port == port:  # Check if the local port matches
                    print(f"Port {port} is already in use by process {proc.info['name']} (PID: {proc.info['pid']}).")
                    print(f"Terminating process {proc.info['name']} (PID: {proc.info['pid']})...\n")
                    proc.terminate()  # Terminate the process using the port
                    proc.wait()  # Wait for the process to terminate
                    print(f"Process {proc.info['name']} (PID: {proc.info['pid']}) terminated successfully.\n")
                    return True
        except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess, AttributeError):
            # Handle processes without connections or processes that might have no access to certain details
            continue
    return False

def main():
    # Parse command-line arguments
    parser = argparse.ArgumentParser(description="Run Docker container and flash ESP32.")
    parser.add_argument('--flash', action='store_true', help="Flash the ESP32 after starting the server.")
    parser.add_argument('--start-container', action='store_true', help="Start the Docker container without flashing.")
    parser.add_argument('--non-interactive', action='store_true', help="Start the RFC2217 server and PowerShell script in the background.")
    args = parser.parse_args()

    # Check if port 4000 is in use, and terminate the process if necessary
    if check_and_terminate_port(4000):
        print("Port 4000 was in use and has been terminated. Now proceeding with the RFC2217 server...\n")
    else:
        print("Port 4000 is free.\n")

    # If --non-interactive is provided, run the PowerShell script in the background
    if args.non_interactive:
        run_power_shell_script()

    # List available serial ports
    ports = list_serial_ports()
    if not ports:
        print("No serial ports found.")
        if args.start_container:
            run_docker_command()
        else:
            print("Please connect the device and run the script again.")
            sys.exit(0)  # Exit if no serial ports are found and no container should be started
    
    else:
        print("Available serial ports:")
        for port in ports:
            print(f" - {port.device}: {port.description}")
        
        match_found = False
        for port in ports:
            if "Silicon Labs CP210x USB to UART Bridge" in port.description:
                print(f"Match found: {port.device} -> {port.description}")
                match_found = True
                # Run RFC2217 server before entering Docker container
                run_rfc2217_server(port.device)
                break
        
        if not match_found:
            if args.start_container:
                run_docker_command()
            else:
                print("No matching serial port found. Please connect the device and run the script again.")
                sys.exit(0)
        else:
            if args.flash:
                run_docker_command(is_flash=True)
            elif args.start_container:
                run_docker_command()

if __name__ == "__main__":
    main()
