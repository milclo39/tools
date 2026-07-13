@echo off
pyinstaller --onefile --noconsole --icon=app_icon.ico --add-data "app_icon.ico;." %1
::pyinstaller --onefile --noconsole --icon=app_icon.ico --add-data "app_icon.ico;." --add-data "images/tako1.png;." %1