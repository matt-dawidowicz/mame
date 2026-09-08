focus maincpu
logerror "CDI BIOS PROGRESS PROBE START\n"
temp0=0
bpset 404d92:maincpu,++temp0==1,{ logerror "FIRST_SCAN PC=%08X A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X D0=%08X D1=%08X D2=%08X D3=%08X D4=%08X D5=%08X D6=%08X D7=%08X SP=%08X SR=%04X\n",pc,a0,a1,a2,a3,a4,a5,a6,d0,d1,d2,d3,d4,d5,d6,d7,sp,sr ; g }
gtime #5000
logerror "T05 PC=%08X A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X D0=%08X D1=%08X D2=%08X D3=%08X SP=%08X SR=%04X W_A2=%04X\n",pc,a0,a1,a2,a3,a4,a5,a6,d0,d1,d2,d3,sp,sr,w@a2
gtime #5000
logerror "T10 PC=%08X A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X D0=%08X D1=%08X D2=%08X D3=%08X SP=%08X SR=%04X W_A2=%04X\n",pc,a0,a1,a2,a3,a4,a5,a6,d0,d1,d2,d3,sp,sr,w@a2
gtime #5000
logerror "T15 PC=%08X A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X D0=%08X D1=%08X D2=%08X D3=%08X SP=%08X SR=%04X W_A2=%04X\n",pc,a0,a1,a2,a3,a4,a5,a6,d0,d1,d2,d3,sp,sr,w@a2
gtime #5000
logerror "T20 PC=%08X A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X D0=%08X D1=%08X D2=%08X D3=%08X SP=%08X SR=%04X W_A2=%04X\n",pc,a0,a1,a2,a3,a4,a5,a6,d0,d1,d2,d3,sp,sr,w@a2
gtime #5000
logerror "T25 PC=%08X A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X D0=%08X D1=%08X D2=%08X D3=%08X SP=%08X SR=%04X W_A2=%04X\n",pc,a0,a1,a2,a3,a4,a5,a6,d0,d1,d2,d3,sp,sr,w@a2
gtime #5000
logerror "T30 PC=%08X A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X D0=%08X D1=%08X D2=%08X D3=%08X SP=%08X SR=%04X W_A2=%04X\n",pc,a0,a1,a2,a3,a4,a5,a6,d0,d1,d2,d3,sp,sr,w@a2
gtime #5000
logerror "T35 PC=%08X A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X D0=%08X D1=%08X D2=%08X D3=%08X SP=%08X SR=%04X W_A2=%04X\n",pc,a0,a1,a2,a3,a4,a5,a6,d0,d1,d2,d3,sp,sr,w@a2
gtime #5000
logerror "T40 PC=%08X A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X D0=%08X D1=%08X D2=%08X D3=%08X SP=%08X SR=%04X W_A2=%04X\n",pc,a0,a1,a2,a3,a4,a5,a6,d0,d1,d2,d3,sp,sr,w@a2
gtime #5000
logerror "T45 PC=%08X A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X D0=%08X D1=%08X D2=%08X D3=%08X SP=%08X SR=%04X W_A2=%04X\n",pc,a0,a1,a2,a3,a4,a5,a6,d0,d1,d2,d3,sp,sr,w@a2
gtime #5000
logerror "T50 PC=%08X A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X D0=%08X D1=%08X D2=%08X D3=%08X SP=%08X SR=%04X W_A2=%04X\n",pc,a0,a1,a2,a3,a4,a5,a6,d0,d1,d2,d3,sp,sr,w@a2
gtime #5000
logerror "T55 PC=%08X A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X D0=%08X D1=%08X D2=%08X D3=%08X SP=%08X SR=%04X W_A2=%04X\n",pc,a0,a1,a2,a3,a4,a5,a6,d0,d1,d2,d3,sp,sr,w@a2
gtime #5000
logerror "T60 PC=%08X A0=%08X A1=%08X A2=%08X A3=%08X A4=%08X A5=%08X A6=%08X D0=%08X D1=%08X D2=%08X D3=%08X SP=%08X SR=%04X W_A2=%04X\n",pc,a0,a1,a2,a3,a4,a5,a6,d0,d1,d2,d3,sp,sr,w@a2
trace cdi_bios_tail.tr,maincpu
gtime #20
traceflush
trace off,maincpu
logerror "PROBE_DONE PC=%08X A2=%08X A3=%08X SP=%08X SR=%04X\n",pc,a2,a3,sp,sr
quit
