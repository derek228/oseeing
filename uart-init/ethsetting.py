import tkinter as tk
from tkinter import ttk
import configparser
import ir8062_config
import serial.tools.list_ports
from tkinter import messagebox

def apply_settings():
    apply_button.focus_set()
    #save_button.grid(sticky=tk.S)
    save_button.grid(row=14,column=2)

def download_start():
    ser = serial.Serial(eth.com_port,115200)
    file_path = "ethernet.ini"
    with open(file_path, 'rb') as file:
        file_data = file.read()
    print(file_data)
    ser.write(file_data)
    ser.close()

def connect_device():
    global mac_result
    global ip_result
    ser = serial.Serial(eth.com_port,115200)
    state=0;
    while state==0 :
        data = ser.readline()
        print(data)
        ser.write(data)
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
            download_button.grid(row=15,column=1)
            state = 2
            break
        else :
            print(f'Connection failure...{data}')
    ser.close()

def save_settings():
    print(eth.com_port)
    config = configparser.RawConfigParser()
    # 建立各區段及其對應的值
    with open('ethernet.ini', 'w') as file:
        file.write("\n")
    config['CONNECTIVITY'] = {
    	'MODE':'RJ45',
    	'DATA':eth.transfer_format,
        'ALARM':eth.alarm,
        'OVER_TEMPERATURE':eth.over_temperature,
        'UNDER_TEMPERATURE':eth.under_temperature,
        'ALERT_TEMPERATURE':eth.alert_temperature,
        'do1on':rs485.do1on,
        'do1off':rs485.do1off,
        'do2on':rs485.do2on,
        'do2off':rs485.do2off
    	}
    with open('ethernet.ini', 'a') as configfile:
        config.write(configfile)
    with open('ethernet.ini', 'a') as file:
        file.write("END_OF_FILE\n")
    print("Settings saved to settings.ini")
    save_button.grid_forget()
def rj45_alarm_on_click():
#    eth.alarm=rj45_alarm_var.get()
    eth.set_alarm(rj45_alarm_var.get())
    #print(eth)

def rj45_alarm_off_click():
#    eth.alarm=rj45_alarm_var.get()
    eth.set_alarm(rj45_alarm_var.get())
    #print(eth)

def rj45_full_data_click():
    eth.set_transfer_format(rj45_data_format.get())

def rj45_simple_data_click():
    eth.set_transfer_format(rj45_data_format.get())

def scan_serial_ports():
    ports = serial.tools.list_ports.comports()  # scan serial port
    return [port.device for port in ports]  #  retrun available device

def select_port(selected_port):
    eth.set_serial(selected_port)
    com_port_label.config(text=f"COM port device : {selected_port}")
    print(f"Selece COM port: {selected_port}")

def over_temperature_on_leave(event) :
    over=rj45_over_temperature_entry.get()
    if not over.isdigit():
        messagebox.showwarning("Warning", "Please input digital number")
    else:
        #messagebox.showinfo("Warning", f"Over Temperature is: {over}")
        eth.set_over_temperature(over)
def under_temperature_on_leave(event) :
    under=rj45_under_temperature_entry.get()
    if not under.isdigit():
        messagebox.showwarning("Warning", "Please input digital number")
    else:
        #messagebox.showinfo("Warning", f"Under Temperature is: {under}")
        eth.set_under_temperature(under)
def alert_temperature_on_leave(event) :
    alert=rj45_alert_temperature_entry.get()
    if not alert.isdigit():
        messagebox.showwarning("Warning", "Please input digital number")
    else:
        #messagebox.showinfo("Warning", f"Under Temperature is: {under}")
        eth.set_alert_temperature(alert)

def rs485_do1on_command(event) :
    do1on=rs485_do1on_entry.get()
    rs485.set_do1on(do1on)

def rs485_do1off_command(event) :
    do1off=rs485_do1off_entry.get()
    rs485.set_do1off(do1off)

def rs485_do2on_command(event) :
    do2on=rs485_do2on_entry.get()
    rs485.set_do2on(do2on)

def rs485_do2off_command(event) :
    do2off=rs485_do2off_entry.get()
    rs485.set_do2off(do2off)

def parse_default_ini () :
    ini_config = configparser.ConfigParser()
    try :
        ini_config.read('ethernet.ini')
    except Exception as e:
        print(f"ERROR : {e}")
    sections = ini_config.sections()
    print('Sections:', sections)
    for section in sections:
        print('Section:', section)
        options = ini_config.options(section)
        if section == "CONNECTIVITY" :
            for option in options:
                value = ini_config.get(section, option)
                if option == 'mode' :
                    eth.transfer_port = value
                elif option == 'data' :
                    eth.transfer_format = value
                elif option == 'alarm' :
                    eth.alarm = value
                elif option == 'over_temperature' :
                    eth.over_temperature = value
                elif option == 'under_temperature' :
                    eth.under_temperature = value 
                elif option == 'alert_temperature' :
                    eth.alert_temperature = value
                elif option == 'do1on' :
                    rs485.do1on = value
                elif option == 'do1off' :
                    rs485.do1off = value
                elif option == 'do2off' :
                    rs485.do2off = value
                elif option == 'do2on' :
                    rs485.do2on = value
                print(f'{option} = {value}')    

def print_ini_setting() :
    print(f"transfer_port = {eth.transfer_port}")
    print(f"transfer_format = {eth.transfer_format}")
    print(f"alarm = {eth.alarm}")
    print(f"over temperature = {eth.over_temperature}")
    print(f"under temperature = {eth.under_temperature}")
    print(f"alert_temperature = {eth.alert_temperature}")
    print(f"DO 1 On : {rs485.do1on}")
    print(f"DO 1 Off : {rs485.do1off}")
    print(f"DO2 On : {rs485.do2on}")
    print(f"DO2 Off : {rs485.do2off}")

eth=ir8062_config.EthernetConfig()
rs485=ir8062_config.RS485CommandConfig()
serial_ports = scan_serial_ports()
global mac_result
global ip_reslut 
ip_result= "None"
mac_result = "None"

print(serial_ports)
parse_default_ini()
print_ini_setting()
# 建立主視窗
root = tk.Tk()
root.title("Settings")
root.geometry('800x600')
root.resizable(True,True)

# 建立 COM Port 標籤和文字輸入欄
menu = tk.Menu(root)
serial_menu = tk.Menu(menu, tearoff=0)
for port in serial_ports:
    serial_menu.add_command(label=port, command=lambda p=port: select_port(p))
    
menu.add_cascade(label="Select Serial Ports Device Name.....", menu=serial_menu)
root.config(menu=menu)
 
com_port_label = tk.Label(root, text="COM Port:")
com_port_label.grid(row=0,column=0,sticky=tk.W)
# 建立 Transfer Port 標籤和 Checkbutton
#transfer_port_label = tk.Label(root, text="Transfer Port:")
#transfer_port_label.grid(row=1,column=0,sticky=tk.W)
#transfer_port_var = tk.StringVar()
#rj45_checkbox = tk.Checkbutton(root, text="RJ45", variable=transfer_port_var, onvalue="RJ45", offvalue="")
#rs485_checkbox = tk.Checkbutton(root, text="RS485", variable=transfer_port_var, onvalue="RS485", offvalue="")
#rj45_checkbox.grid(row=1,column=1,sticky=tk.W)
#rs485_checkbox.grid(row=1,column=2,sticky=tk.W)

# data format for RJ45
rj45_data_format_label = tk.Label(root, text="RJ45 Data Format:")
rj45_data_format_label.grid(row=1,column=0,sticky=tk.W)
rj45_data_format = tk.StringVar(value=eth.transfer_format)
print(rj45_data_format.get())
rj45_full_data_checkbox = tk.Checkbutton(root, text="FULL", variable=rj45_data_format, onvalue="FULL",offvalue="",command=rj45_full_data_click)
rj45_simple_data_checkbox = tk.Checkbutton(root, text="SIMPLE", variable=rj45_data_format, onvalue="SIMPLE",offvalue="",command=rj45_simple_data_click)
rj45_full_data_checkbox.grid(row=1,column=1,sticky=tk.W)
rj45_simple_data_checkbox.grid(row=1,column=2,sticky=tk.W)
rj45_full_data_checkbox.bind("<Button-1>",lambda event: rj45_full_data_click())
rj45_simple_data_checkbox.bind("<Button-1>",lambda event: rj45_simple_data_click())

# alarm for RJ45
rj45_alarm_label = tk.Label(root, text="RJ45 Alarm:")
rj45_alarm_label.grid(row=3,column=0,sticky=tk.W)
rj45_alarm_var = tk.StringVar(value=eth.alarm)
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
rj45_over_temperature_entry.insert(0, eth.over_temperature)
rj45_over_temperature_entry.bind("<FocusOut>", over_temperature_on_leave)

rj45_under_temperature_label = tk.Label(root, text="RJ45 Under Temperature Alarm:")
rj45_under_temperature_label.grid(row=5,column=0,sticky=tk.W)
rj45_under_temperature_entry = tk.Entry(root)
rj45_under_temperature_entry.grid(row=5,column=1)
rj45_under_temperature_entry.insert(0,eth.under_temperature)
rj45_under_temperature_entry.bind("<FocusOut>", under_temperature_on_leave)

rj45_alert_temperature_label = tk.Label(root, text="RJ45 Alert Temperature:")
rj45_alert_temperature_label.grid(row=6,column=0,sticky=tk.W)
rj45_alert_temperature_entry = tk.Entry(root)
rj45_alert_temperature_entry.grid(row=6,column=1)
rj45_alert_temperature_entry.insert(0,eth.alert_temperature)
rj45_alert_temperature_entry.bind("<FocusOut>", alert_temperature_on_leave)

rs485_do1on_label = tk.Label(root, text="RS485 DO 1 On Command:")
rs485_do1on_label.grid(row=7,column=0,sticky=tk.W)
rs485_do1on_entry = tk.Entry(root, width = 50)
rs485_do1on_entry.grid(row=7,column=1)
rs485_do1on_entry.insert(0,rs485.do1on)
rs485_do1on_entry.bind("<FocusOut>", rs485_do1on_command)

rs485_do1off_label = tk.Label(root, text="RS485 DO 1 Off Command:")
rs485_do1off_label.grid(row=8,column=0,sticky=tk.W)
rs485_do1off_entry = tk.Entry(root, width = 50)
rs485_do1off_entry.grid(row=8,column=1)
rs485_do1off_entry.insert(0,rs485.do1off)
rs485_do1off_entry.bind("<FocusOut>", rs485_do1off_command)

rs485_do2on_label = tk.Label(root, text="RS485 DO 2 On Command:")
rs485_do2on_label.grid(row=9,column=0,sticky=tk.W)
rs485_do2on_entry = tk.Entry(root, width = 50)
rs485_do2on_entry.grid(row=9,column=1)
rs485_do2on_entry.insert(0,rs485.do2on)
rs485_do2on_entry.bind("<FocusOut>", rs485_do2on_command)

rs485_do2off_label = tk.Label(root, text="RS485 DO 2 Off Command:")
rs485_do2off_label.grid(row=10,column=0,sticky=tk.W)
rs485_do2off_entry = tk.Entry(root, width = 50)
rs485_do2off_entry.grid(row=10,column=1)
rs485_do2off_entry.insert(0,rs485.do2off)
rs485_do2off_entry.bind("<FocusOut>", rs485_do2off_command)


apply_button = tk.Button(root, text="Apply Settings", command=apply_settings)
apply_button.grid(row=14,column=0)

connect_button = tk.Button(root, text="Connect Device", command=connect_device)
connect_button.grid(row=15,column=0)
download_button = tk.Button(root, text="Download", command=download_start)
download_button.grid(row=15,column=1)
download_button.grid_forget()
mac_label = tk.Label(root, text=f"MAC:{mac_result}")
mac_label.grid(row=16,column=0)
ip_label = tk.Label(root, text=f"IP:{ip_result}")
ip_label.grid(row=17,column=0)

# 建立儲存按鈕
save_button = tk.Button(root, text="Save Settings", command=save_settings)
#save_button.grid(sticky=tk.S)#row=20,column=3)
save_button.grid(row=20,column=0)
save_button.grid_forget()

# 啟動主迴圈
root.mainloop()

