@echo off & setlocal enabledelayedexpansion


echo 开始清除所有临时文件文件

	for /f "delims=" %%p in ('dir /ad /s /b "*debug*" "*release*" "*x64*" "*win32*" "ipch"') do (
		
		echo "%%p"|findstr "运行" >nul
		if !errorlevel!==1 (
			echo 删除"%%p"
			rd /s /q "%%p"
		)
	)
	
	rd /s /q x64
	rd /s /q win32
	
	del /f /s /a:h /q .\*.suo
	del /f /s /q .\*.suo
	del /f /s /q .\*.aps
	del /f /s /a:h /q .\*.ncb
	del /f /s /q .\*.ncb
	del /f /s /a:h /q .\*.sdf
	del /f /s /q .\*.sdf
	del /f /s /a:h /q .\*.VC.db
	del /f /s /q .\*.VC.db
	
	rd /s /q ipch
	rd /s /q .vs
	
echo 清除文件完成！  
	 

echo. & pause