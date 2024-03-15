import tkinter as tk
from tkinter import ttk
import configparser
import ir8062_config
import serial.tools.list_ports
from tkinter import messagebox
import socket
import time
import os

def download_start():
    BUFFER_SIZE =  10240 #1048576; # 1MByte
    #ipaddr = '192.168.100.35'
    port = 8003 #8080
    img_file = "uImage"
    # Get upload file size
    file_size = os.path.getsize(img_file) 
    bytes_sent = 0
    print(f"Start download image {ip_result}")
    print(file_size)
    # Create TCP socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        # connect to server
        #s.connect((ipaddr, port))
        try :
            s.connect((ip_result.strip(), port))
        except Exception as e:
            print(f"An error occurred(socket connect):\n {e}")
            return

        # open upload file
        with open(img_file, 'rb') as file:
            data = file.read(BUFFER_SIZE)
            while data:
                try:
                    s.sendall(data)
                    bytes_sent += len(data)
                    progress = (bytes_sent / file_size) * 100
                    progress_bar["value"] = progress
                    root.update()
                    print(f"progress= {progress}%  SendByte={bytes_sent}")
                    data = file.read(BUFFER_SIZE)
                except socket.error as e:
                    print(f"Socket error occurred: {e}")
                    return
                except socket.timeout as e:
                    print(f"Socket timeout occurred: {e}")
                    return
                except ConnectionResetError as e:
                    print(f"Connection reset error occurred: {e}")
                    return
                except Exception as e:
                    print(f"An error occurred: {e}")
                    return
        print("File sent successfully.")
        download_button.grid_forget()
    time.sleep(1)
    print("Socket closed")
    
    ser = serial.Serial(eth.com_port,115200)
    ser.reset_output_buffer()
    ser.write(b'IR8062IMGDOWNLOADED\n')
    time.sleep(3)
    while file_size > 0:
        data = ser.readline().decode('utf-8')
        if "IR8062SYSTEMREBOOT" in data :
            file_size=0;
        elif "IR8062UPDATEFAILE" in data :
            print("Error : device upgrade fail, reboot and try again")
            ser.close()
            return
        else :
            print(f"wrong reboot cmd = {data}")
        ser.write(b'IR8062IMGDOWNLOADED\n')
        time.sleep(0.5)
    ser.close()
    print("IR8062 system rebooting")
    

def connect_device():
    global mac_result
    global ip_result
    print(eth.com_port)
    ser = serial.Serial(eth.com_port,115200)
    state=0;
    upgrade_cmd = b'IR8062UPDATE\n'
    while state==0 :
        data = ser.readline()
        print(data)
        #ser.write(data)
        print("SEND IR8062UPDATE COMMAND")
        ser.write(upgrade_cmd)
        # get mac and ip
        strtmp=data.decode('utf-8')
        if 'IR8062_BROADCASTING' in strtmp :
            state=1 # check connect ack
        print(strtmp)
        mac = "MAC"
        ip = 'IP'
        if mac in strtmp:
            index = strtmp.find(mac)  # 获取模式的索引位置
            if index != -1:
                mac_result = strtmp[index + len(mac):]  # 获取模式后面的子字符串
                print("Found mac:", mac_result.strip())
            else:
                print("Pattern found, but unable to extract the substring")
            index = mac_result.find(ip) 
            if index != -1:
                ip_result = mac_result[index + len(ip):]  # 获取模式后面的子字符串
                print("Found ip :", ip_result.strip())
            else:
                print("Pattern found, but unable to extract the substring")
            mac_result=mac_result.replace(f'IP{ip_result}',"")
            print("Found mac:", mac_result.strip())
        else:
             print("Pattern not found")
        # end mac ip parse
    while state == 1 :
        data = ser.readline().decode('utf-8')
        print(data)
        if "IR8062_CONNECTED" in data:
            mac_label.config(text=f"MAC : {mac_result}")
            ip_label.config(text=f"IP : {ip_result}")
            print('Device connected...')
            time.sleep(1)
            download_button.grid(row=15,column=2)
            state = 2
            break
        else :
            print(f'Connection failure...{data}')
    ser.reset_output_buffer()
    ser.close()
    print("Disconnect....")
    # return mac_result, ip_result

def scan_serial_ports():
    ports = serial.tools.list_ports.comports()  # scan serial port
    return [port.device for port in ports]  #  retrun available device

def select_port(selected_port):
    eth.set_serial(selected_port)
    com_port_label.config(text=f"COM port device : {selected_port}")
    print(f"Selece COM port: {selected_port}")

'''
def rj45_alarm_on_click():
#    eth.alarm=rj45_alarm_var.get()
    eth.set_alarm(rj45_alarm_var.get())
    #print(eth)

def over_temperature_on_leave(event) :
    over=rj45_over_temperature_entry.get()
    if not over.isdigit():
        messagebox.showwarning("Warning", "Please input digital number")
    else:
        #messagebox.showinfo("Warning", f"Over Temperature is: {over}")
        eth.set_over_temperature(over)
'''
eth=ir8062_config.EthernetConfig()
rs485=ir8062_config.RS485CommandConfig()
serial_ports = scan_serial_ports()
global mac_result
global ip_reslut 
ip_result= "None"
mac_result = "None"

print(serial_ports)
root = tk.Tk()
root.title("Settings")
root.geometry('800x600')
root.resizable(True,True)

# COM Port lable
menu = tk.Menu(root)
serial_menu = tk.Menu(menu, tearoff=0)
for port in serial_ports:
    serial_menu.add_command(label=port, command=lambda p=port: select_port(p))
    
menu.add_cascade(label="Select Serial Ports Device Name.....", menu=serial_menu)
root.config(menu=menu)
 
com_port_label = tk.Label(root, text="COM Port:")
com_port_label.grid(row=0,column=0,sticky=tk.W)
'''
# alarm for RJ45
rj45_alarm_label = tk.Label(root, text="RJ45 Alarm:")
rj45_alarm_label.grid(row=3,column=0,sticky=tk.W)
rj45_alarm_var = tk.StringVar()
rj45_alarm_on_checkbox = tk.Checkbutton(root, text="On", variable=rj45_alarm_var, onvalue="on",offvalue="",command=rj45_alarm_on_click)
rj45_alarm_off_checkbox = tk.Checkbutton(root, text="Off", variable=rj45_alarm_var, onvalue="off",offvalue="",command=rj45_alarm_off_click)
rj45_alarm_on_checkbox.grid(row=3,column=1,sticky=tk.W)
rj45_alarm_off_checkbox.grid(row=3,column=2,sticky=tk.W)
rj45_alarm_on_checkbox.bind("<Button-1>",lambda event: rj45_alarm_on_click())
rj45_alarm_off_checkbox.bind("<Button-1>",lambda event: rj45_alarm_off_click())

rj45_over_temperature_label = tk.Label(root, text="RJ45 Over Temperature Alarm:")
rj45_over_temperature_label.grid(row=4,column=0,sticky=tk.W)
rj45_over_temperature_entry = tk.Entry(root)
rj45_over_temperature_entry.grid(row=4,column=1)
rj45_over_temperature_entry.bind("<FocusOut>", over_temperature_on_leave)
'''
connect_button = tk.Button(root, text="Connect Device", command=connect_device)
connect_button.grid(row=15,column=0)
download_button = tk.Button(root, text="Download", command=download_start)
download_button.grid(row=15,column=1)
download_button.grid_forget()
mac_label = tk.Label(root, text=f"MAC:{mac_result}")
mac_label.grid(row=16,column=0)
ip_label = tk.Label(root, text=f"IP:{ip_result}")
ip_label.grid(row=17,column=0)
progress_bar = ttk.Progressbar(root, mode="determinate", length=200)
progress_bar["value"]=0
progress_bar.grid(row=18, column=0, columnspan=2, pady=10)

root.mainloop()

