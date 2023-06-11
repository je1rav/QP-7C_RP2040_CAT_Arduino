import serial
import glob,os,re,time
import tkinter as tk
from tkinter import ttk
from tkinter import messagebox

#----------------------------------------------------------------
# #CAT Control of RP2040 with serial (Frequency change)
#----------------------------------------------------------------

def freq_change():
    try:
        status_label["text"] = ""
        status_label.update()
        rig_path =rig_path_box.get()
        freq = frequency_box.get()
        ser = serial.Serial("COM"+rig_path,115200, timeout=0.1)
        send1 = "00000000"+ freq
        send = "FA" + send1[-8:]
        send += "000;"
        send = send.encode('utf-8')
        ser.write(send)
        time.sleep(0.3)
        receive = ser.read_all()
        if receive != send:
            status_label["background"] = "red"
            status_label["text"] = "False"
        else:
            status_label["background"] = "green"
            status_label["text"] = "Success"
    except:
        messagebox.showerror('ERROR')
        frequency_box.delete(0, tk.END)
    ser.close()


#----------------------------------------------------------------
# GUI
#----------------------------------------------------------------
main_win = tk.Tk()
main_win.title("CAT Control QP-7C")
main_win.geometry("270x130")

frame1 = ttk.Frame(main_win)
frame1.grid(column=0, row=0, sticky=tk.NSEW, padx=5, pady=10)

rig_path_label = ttk.Label(frame1, text="       RIG  Serail port           COM")
rig_path_box = ttk.Entry(frame1)
rig_path_box.insert(tk.END, "")

frequency_label = ttk.Label(frame1, text="        Frequerncy    (kHz)        ")
frequency_box = ttk.Entry(frame1)
frequency_box.insert(tk.END, "")

status_label = ttk.Label(frame1, text="", foreground ="white")
status_box = ttk.Entry(frame1)
status_box.insert(tk.END, "")

app_btn = ttk.Button(frame1, text="SEND", width=12, command=freq_change)

rig_path_label.grid(column=0, row=0, pady=10)
rig_path_box.grid(column=1, row=0, sticky=tk.EW, padx=5)
frequency_label.grid(column=0, row=1, pady=10)
frequency_box.grid(column=1, row=1, sticky=tk.EW, padx=5)
app_btn.grid(column=1, row=3, padx=10)
status_label.grid(column=0, row=3, pady=10)

main_win.columnconfigure(0, weight=1)
main_win.rowconfigure(0, weight=1)
frame1.columnconfigure(1, weight=1)

main_win.mainloop() 