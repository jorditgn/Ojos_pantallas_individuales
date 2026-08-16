@echo off
title Image to Binary Converter
echo Iniciando aplicacion...

:: Asegura que la dependencia Pillow este instalada (modo silencioso)
pip install Pillow -q

:: Ejecuta el programa
python gui-image-tools.py

:: Pausa la consola para que no se cierre sola si hay un error
pause