focus maincpu
logerror "CDI BIOS STALL PROBE START\n"
temp0=0
bpset 404d92:maincpu,++temp0==1,{ logerror "SCAN_FIRST PC=%08X A2=%08X A3=%08X REM=%08X D0=%08X D1=%08X D2=%08X D3=%08X SR=%04X W_A2=%04X W_END=%04X\n",pc,a2,a3,a3-a2,d0,d1,d2,d3,sr,w@a2,w@(a3-2) ; g }
gtime #5000
logerror "PROBE_END PC=%08X A2=%08X A3=%08X REM=%08X D0=%08X D1=%08X D2=%08X D3=%08X SR=%04X W_A2=%04X W_END=%04X\n",pc,a2,a3,a3-a2,d0,d1,d2,d3,sr,w@a2,w@(a3-2)
quit
