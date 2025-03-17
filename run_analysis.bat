@echo off
echo Running MSBuild...
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" C:\Users\danie\source\repos\4_6_1\4_6_1.sln /t:Rebuild /v:diag /fl /flp:logfile=C:\Users\danie\source\repos\4_6_1\errors.log

echo Running Error Analyzer...
ErrorAnalyzer.exe

echo Process Complete!
pause
