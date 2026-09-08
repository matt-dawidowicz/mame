focus maincpu
logerror "CDI 7TH GUEST TRANSPORT PROBE START\n"

// Log guest writes that start/configure CDIC transport and audio.  These are
// debugger watchpoints only; they do not modify guest memory.
wp 303c00:maincpu,2,w,1,{ logerror "CDIC_W CMD pc=%08X addr=%08X data=%08X\n",pc,wpaddr,wpdata ; g }
wp 303c02:maincpu,c,w,1,{ logerror "CDIC_W CFG pc=%08X addr=%08X data=%08X\n",pc,wpaddr,wpdata ; g }
wp 303c80:maincpu,2,w,1,{ logerror "CDIC_W DSEL pc=%08X data=%08X\n",pc,wpdata ; g }
wp 303ff4:maincpu,c,w,1,{ logerror "CDIC_W CTRL pc=%08X addr=%08X data=%08X\n",pc,wpaddr,wpdata ; g }

// Log only the DVC control registers involved in starting/selecting playback;
// deliberately avoid the DSP bootstrap windows, which are written thousands of
// times during cartridge initialisation.
wp e01000:maincpu,2,w,1,{ logerror "DVC_W VCD pc=%08X data=%08X\n",pc,wpdata ; g }
wp e03000:maincpu,2,w,1,{ logerror "DVC_W FMA_CMD pc=%08X data=%08X\n",pc,wpdata ; g }
wp e03008:maincpu,2,w,1,{ logerror "DVC_W FMA_STREAM pc=%08X data=%08X\n",pc,wpdata ; g }
wp e0301c:maincpu,2,w,1,{ logerror "DVC_W FMA_IE pc=%08X data=%08X\n",pc,wpdata ; g }
wp e04060:maincpu,2,w,1,{ logerror "DVC_W FMV_IE pc=%08X data=%08X\n",pc,wpdata ; g }
wp e0408c:maincpu,2,w,1,{ logerror "DVC_W FMV_VDI pc=%08X data=%08X\n",pc,wpdata ; g }
wp e040c0:maincpu,6,w,1,{ logerror "DVC_W FMV_CTL pc=%08X addr=%08X data=%08X\n",pc,wpaddr,wpdata ; g }

// w@ suppresses read side effects in the MAME debugger, so these snapshots do
// not acknowledge ABUF/XBUF/AUDCTL or DVC IRQ status.
gtime #10000
logerror "T10 PC=%08X SR=%04X CDIC_CMD=%04X TIME=%04X:%04X FILE=%04X CH=%04X:%04X ACH=%04X DSEL=%04X ABUF=%04X XBUF=%04X DMA=%04X AUDCTL=%04X IVEC=%04X DBUF=%04X FMA_CMD=%04X FMA_ST=%04X FMV_IN=%04X FMV_IE=%04X FMV_IRQ=%04X FMV_SYS=%04X FMV_VID=%04X FMV_STR=%04X\n",pc,sr,w@303c00,w@303c02,w@303c04,w@303c06,w@303c08,w@303c0a,w@303c0c,w@303c80,w@303ff4,w@303ff6,w@303ff8,w@303ffa,w@303ffc,w@303ffe,w@e03000,w@e03002,w@e0405e,w@e04060,w@e04062,w@e040c0,w@e040c2,w@e040c4
gtime #10000
logerror "T20 PC=%08X SR=%04X CDIC_CMD=%04X TIME=%04X:%04X FILE=%04X CH=%04X:%04X ACH=%04X DSEL=%04X ABUF=%04X XBUF=%04X DMA=%04X AUDCTL=%04X IVEC=%04X DBUF=%04X FMA_CMD=%04X FMA_ST=%04X FMV_IN=%04X FMV_IE=%04X FMV_IRQ=%04X FMV_SYS=%04X FMV_VID=%04X FMV_STR=%04X\n",pc,sr,w@303c00,w@303c02,w@303c04,w@303c06,w@303c08,w@303c0a,w@303c0c,w@303c80,w@303ff4,w@303ff6,w@303ff8,w@303ffa,w@303ffc,w@303ffe,w@e03000,w@e03002,w@e0405e,w@e04060,w@e04062,w@e040c0,w@e040c2,w@e040c4
gtime #10000
logerror "T30 PC=%08X SR=%04X CDIC_CMD=%04X TIME=%04X:%04X FILE=%04X CH=%04X:%04X ACH=%04X DSEL=%04X ABUF=%04X XBUF=%04X DMA=%04X AUDCTL=%04X IVEC=%04X DBUF=%04X FMA_CMD=%04X FMA_ST=%04X FMV_IN=%04X FMV_IE=%04X FMV_IRQ=%04X FMV_SYS=%04X FMV_VID=%04X FMV_STR=%04X\n",pc,sr,w@303c00,w@303c02,w@303c04,w@303c06,w@303c08,w@303c0a,w@303c0c,w@303c80,w@303ff4,w@303ff6,w@303ff8,w@303ffa,w@303ffc,w@303ffe,w@e03000,w@e03002,w@e0405e,w@e04060,w@e04062,w@e040c0,w@e040c2,w@e040c4
gtime #10000
logerror "T40 PC=%08X SR=%04X CDIC_CMD=%04X TIME=%04X:%04X FILE=%04X CH=%04X:%04X ACH=%04X DSEL=%04X ABUF=%04X XBUF=%04X DMA=%04X AUDCTL=%04X IVEC=%04X DBUF=%04X FMA_CMD=%04X FMA_ST=%04X FMV_IN=%04X FMV_IE=%04X FMV_IRQ=%04X FMV_SYS=%04X FMV_VID=%04X FMV_STR=%04X\n",pc,sr,w@303c00,w@303c02,w@303c04,w@303c06,w@303c08,w@303c0a,w@303c0c,w@303c80,w@303ff4,w@303ff6,w@303ff8,w@303ffa,w@303ffc,w@303ffe,w@e03000,w@e03002,w@e0405e,w@e04060,w@e04062,w@e040c0,w@e040c2,w@e040c4

trace cdi_7th_guest_transport_tail.tr,maincpu
gtime #20
traceflush
trace off,maincpu
logerror "FINAL PC=%08X SR=%04X CDIC_CMD=%04X TIME=%04X:%04X FILE=%04X CH=%04X:%04X ACH=%04X DSEL=%04X ABUF=%04X XBUF=%04X DMA=%04X AUDCTL=%04X IVEC=%04X DBUF=%04X FMA_CMD=%04X FMA_ST=%04X FMA_IRQ=%04X FMA_IE=%04X FMV_IN=%04X FMV_IE=%04X FMV_IRQ=%04X FMV_VDI=%04X FMV_FIFO=%04X FMV_SYS=%04X FMV_VID=%04X FMV_STR=%04X\n",pc,sr,w@303c00,w@303c02,w@303c04,w@303c06,w@303c08,w@303c0a,w@303c0c,w@303c80,w@303ff4,w@303ff6,w@303ff8,w@303ffa,w@303ffc,w@303ffe,w@e03000,w@e03002,w@e0301a,w@e0301c,w@e0405e,w@e04060,w@e04062,w@e0408c,w@e040a4,w@e040c0,w@e040c2,w@e040c4
quit
