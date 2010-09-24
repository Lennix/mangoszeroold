$search = FileFindFirstFile("*mangos*.sql")
while 1
	$file = FileFindNextFile($search)
	If @Error Then ExitLoop
	$input = FileRead($file)
	FileWriteLine("mangos.sql.new",$input)
WEnd

$search = FileFindFirstFile("*characters*.sql")
while 1
	$file = FileFindNextFile($search)
	If @Error Then ExitLoop
	$input = FileRead($file)
	FileWriteLine("characters.sql.new",$input)
WEnd

$search = FileFindFirstFile("*realmd*.sql")
while 1
	$file = FileFindNextFile($search)
	If @Error Then ExitLoop
	$input = FileRead($file)
	FileWriteLine("realmd.sql.new",$input)
WEnd