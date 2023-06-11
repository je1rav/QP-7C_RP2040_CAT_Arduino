import serial
import tkinter as tk
from tkinter import ttk
from tkinter import messagebox

#----------------------------------------------------------------
# #Connect WSJT-X(com0com) and RIG(RP2040)
#----------------------------------------------------------------


def connect():
    com0com_path = com0com_path_box.get()
    rig_path = rig_path_box.get()
    ser1 = serial.Serial("COM"+com0com_path, 115200)
    ser2 = serial.Serial("COM"+rig_path,115200)
    while True:
        result1 = ser1.read_all()
        if result1 !=b"":
            ser2.write(result1)
        result2 = ser2.read_all()
        if result2 !=b"":
            ser1.write(result2)
    ser1.close()
    ser2.close()

#----------------------------------------------------------------
# GUI
#----------------------------------------------------------------
main_win = tk.Tk()
main_win.title("Connect WSJT-X and RP2040")
main_win.geometry("340x130")

frame1 = ttk.Frame(main_win)
frame1.grid(column=0, row=0, sticky=tk.NSEW, padx=5, pady=10)

com0com_path_label = ttk.Label(frame1, text="       WSJT-X(com0com)         COM")
com0com_path_box = ttk.Entry(frame1)
com0com_path_box.insert(tk.END, "")

rig_path_label = ttk.Label(frame1, text="                   RIG                        COM")
rig_path_box = ttk.Entry(frame1)
rig_path_box.insert(tk.END, "")

app_btn = ttk.Button(frame1, text="RUN", width=12, command=connect)

com0com_path_label.grid(column=0, row=0, pady=10)
com0com_path_box.grid(column=1, row=0, sticky=tk.EW, padx=5)
rig_path_label.grid(column=0, row=1, pady=10)
rig_path_box.grid(column=1, row=1, sticky=tk.EW, padx=5)
app_btn.grid(column=1, row=3, padx=10)

main_win.columnconfigure(0, weight=1)
main_win.rowconfigure(0, weight=1)
frame1.columnconfigure(1, weight=1)

main_win.mainloop() 