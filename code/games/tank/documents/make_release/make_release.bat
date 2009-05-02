
SETLOCAL
SET target_dir=D:\swentw\SW_Deployment_WIN32
SET target_data=%target_dir%\data
SET dest_dir=..\..\

copy %dest_dir%\bin\tankClient.exe %target_dir%

copy %dest_dir%config_common.xml %target_dir%
copy %dest_dir%config_client.xml %target_dir%
copy %dest_dir%config_server.xml %target_dir%

del %target_dir%\CEGUI.log
del %target_dir%\log_client.txt

rd %target_data% /S /Q

mkdir %target_data%

xcopy %dest_dir%data %target_data% /Y /S /EXCLUDE:exclude.txt

pause