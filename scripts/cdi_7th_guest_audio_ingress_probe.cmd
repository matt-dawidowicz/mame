focus maincpu
logerror "CDI 7TH GUEST AUDIO INGRESS PROBE START\n"

wp e03000:maincpu,2,w,1,{ logerror "FMA_W CMD pc=%08X data=%08X\n",pc,wpdata ; g }
wp e03008:maincpu,2,w,1,{ logerror "FMA_W STREAM pc=%08X data=%08X\n",pc,wpdata ; g }
wp e0301c:maincpu,2,w,1,{ logerror "FMA_W IE pc=%08X data=%08X\n",pc,wpdata ; g }
wp e040c0:maincpu,2,w,1,{ logerror "FMV_W SYSCMD pc=%08X data=%08X\n",pc,wpdata ; g }
wp 80004040:maincpu,10,w,1,{ logerror "DMA2_W pc=%08X addr=%08X data=%08X\n",pc,wpaddr,wpdata ; g }


gtime #20000
logerror "A20 PC=%08X FMA_CMD=%04X FMA_ST=%04X FMA_STR=%04X FMA_CUR=%04X FMA_IRQ=%04X FMA_IE=%04X FMV_SYS=%04X FMV_VID=%04X FMV_FIFO=%04X\n",pc,w@e03000,w@e03002,w@e03008,w@e0300a,w@e0301a,w@e0301c,w@e040c0,w@e040c2,w@e040a4

gtime #5000
logerror "A25 PC=%08X FMA_CMD=%04X FMA_ST=%04X FMA_STR=%04X FMA_CUR=%04X FMA_IRQ=%04X FMA_IE=%04X FMV_SYS=%04X FMV_VID=%04X FMV_FIFO=%04X\n",pc,w@e03000,w@e03002,w@e03008,w@e0300a,w@e0301a,w@e0301c,w@e040c0,w@e040c2,w@e040a4

gtime #5000
logerror "A30 PC=%08X FMA_CMD=%04X FMA_ST=%04X FMA_STR=%04X FMA_CUR=%04X FMA_IRQ=%04X FMA_IE=%04X FMV_SYS=%04X FMV_VID=%04X FMV_FIFO=%04X\n",pc,w@e03000,w@e03002,w@e03008,w@e0300a,w@e0301a,w@e0301c,w@e040c0,w@e040c2,w@e040a4

gtime #5000
logerror "A35 PC=%08X FMA_CMD=%04X FMA_ST=%04X FMA_STR=%04X FMA_CUR=%04X FMA_IRQ=%04X FMA_IE=%04X FMV_SYS=%04X FMV_VID=%04X FMV_FIFO=%04X\n",pc,w@e03000,w@e03002,w@e03008,w@e0300a,w@e0301a,w@e0301c,w@e040c0,w@e040c2,w@e040a4

gtime #5000
logerror "A40 PC=%08X FMA_CMD=%04X FMA_ST=%04X FMA_STR=%04X FMA_CUR=%04X FMA_IRQ=%04X FMA_IE=%04X FMV_SYS=%04X FMV_VID=%04X FMV_FIFO=%04X\n",pc,w@e03000,w@e03002,w@e03008,w@e0300a,w@e0301a,w@e0301c,w@e040c0,w@e040c2,w@e040a4

gtime #5000
logerror "A45 PC=%08X FMA_CMD=%04X FMA_ST=%04X FMA_STR=%04X FMA_CUR=%04X FMA_IRQ=%04X FMA_IE=%04X FMV_SYS=%04X FMV_VID=%04X FMV_FIFO=%04X\n",pc,w@e03000,w@e03002,w@e03008,w@e0300a,w@e0301a,w@e0301c,w@e040c0,w@e040c2,w@e040a4

logerror "AUDIO INGRESS FINAL PC=%08X FMA_CMD=%04X FMA_ST=%04X FMA_STR=%04X FMA_CUR=%04X FMA_IRQ=%04X FMA_IE=%04X FMV_SYS=%04X FMV_VID=%04X FMV_FIFO=%04X\n",pc,w@e03000,w@e03002,w@e03008,w@e0300a,w@e0301a,w@e0301c,w@e040c0,w@e040c2,w@e040a4
quit
