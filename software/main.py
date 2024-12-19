import sys
from PyQt6.QtGui import QGuiApplication
from PyQt6.QtQml import QQmlApplicationEngine
from core.process_monitor import get_running_processes
from core.usb_communication import send_message_to_keyboard

if __name__ == "__main__":
    app = QGuiApplication(sys.argv)
    
    # Initialize QML engine
    engine = QQmlApplicationEngine()
    engine.load("ui/main.qml")
    if not engine.rootObjects():
        sys.exit(-1)

    # grab running processes
    processes = get_running_processes()
    #print("Running processes:", processes)

    if "cs2.exe" in processes:
        print("Counter-Strike 2 is running!")
        #Send down info
        send_message_to_keyboard([0x01])  # Disable Windows key
    
    sys.exit(app.exec())