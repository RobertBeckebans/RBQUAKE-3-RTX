REM astyle.exe -v --formatted --options=astyle-options.ini --exclude="code/libs" --recursive *.h
REM astyle.exe -v --formatted --options=astyle-options.ini --exclude="code/libs" --recursive *.c
REM astyle.exe -v --formatted --options=astyle-options.ini --exclude="code/libs" --recursive *.cpp
REM astyle.exe -v --formatted --options=astyle-options.ini --exclude="code/libs" --recursive *.glsl
REM astyle.exe -v --formatted --options=astyle-options.ini --exclude="code/libs" --recursive *.comp
REM astyle.exe -v --formatted --options=astyle-options.ini --exclude="code/libs" --recursive *.vert
REM astyle.exe -v --formatted --options=astyle-options.ini --exclude="code/libs" --recursive *.frag

REM astyle.exe -v --formatted --options=astyle-options.ini shared\q_shared.h
REM astyle.exe -v --formatted --options=astyle-options.ini engine\renderer\tr_local.h

astyle.exe -v -Q --options=astyle-options.ini --recursive ../baseq3/*.hlsl

pause