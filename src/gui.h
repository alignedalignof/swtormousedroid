#define SMD_GUI_W_BORDER_L		2
#define SMD_GUI_W_BORDER_R		2
#define SMD_GUI_W_FRAME			500
#define SMD_GUI_W 				(SMD_GUI_W_BORDER_L \
								+ SMD_GUI_W_FRAME \
								+ SMD_GUI_W_BORDER_R)
#define SMD_GUI_W_XMIN_BB		47
#define SMD_GUI_W_XMIN			11
#define SMD_GUI_W_TOGL			20
#define SMD_GUI_W_BINDER		(SMD_GUI_W_FRAME/4)

#define SMD_GUI_H_TITLE			30
#define SMD_GUI_H_SBTTL			20
#define SMD_GUI_H_BIND			80
#define SMD_GUI_H_BINDS			(6*SMD_GUI_H_BIND)
#define SMD_GUI_H_ACTV			50
#define SMD_GUI_H_XTRA			100
#define SMD_GUI_H_BORDER_B		2
#define SMD_GUI_H 				(SMD_GUI_H_TITLE \
								+ 2*SMD_GUI_H_SBTTL \
								+ SMD_GUI_H_BINDS \
								+ SMD_GUI_H_ACTV \
								+ SMD_GUI_H_XTRA \
								+ SMD_GUI_H_BORDER_B)
#define SMD_GUI_H_BINDER		20

#define SMD_GUI_OFS_ULINE		(24 + SMD_GUI_W_BORDER_L)
#define SMD_GUI_OFS_XMIN		((SMD_GUI_W_XMIN_BB - SMD_GUI_W_XMIN)/2)
#define SMD_GUI_OFS_OPTS_V		(SMD_GUI_H_TITLE + 2*SMD_GUI_H_SBTTL + SMD_GUI_H_ACTV + SMD_GUI_H_BINDS + SMD_GUI_W_TOGL/2)
#define SMD_GUI_OFS_SPACE		((SMD_GUI_H_BIND - 2*SMD_GUI_H_BINDER)/3)
#define SMD_GUI_OFS_ACTV		(SMD_GUI_H_TITLE + SMD_GUI_H_BINDS)
#define SMD_GUI_OFS_OPTS		(SMD_GUI_OFS_ACTV + SMD_GUI_H_SBTTL + SMD_GUI_H_ACTV)

#define SMD_GUI_RGB_VOID		(RGB(0, 6, 9))
#define SMD_GUI_RGB_BK			(RGB(0, 32, 49))
#define SMD_GUI_RGB_X			(RGB(232, 17, 35))
#define SMD_GUI_RGB_MIN			(RGB(0x1a, 0x75, 0x91))
#define SMD_GUI_RGB_ULINE		(RGB(8, 70, 102))
#define SMD_GUI_RGB_OLINE		(RGB(14, 82, 122))
#define SMD_GUI_RGB_HLINE		(RGB(172, 239, 252))
#define SMD_GUI_RGB_FLINE		(RGB(172, 239, 252))
#define SMD_GUI_RGB_HOT			(RGB(244, 196, 66))
#define SMD_GUI_RGB_TTGL		(RGB(227, 227, 186))
#define SMD_GUI_RGB_BTGL		(RGB(177, 132, 22))
#define SMD_GUI_RGB_SMD			(RGB(180, 255, 0))

#define SMD_GUI_TXT_TITLE		"SWToR Mouse Droid"
#define SMD_GUI_TXT_TITLE_GUI	"SWToR Mouse Droid (GUI only)"
#define SMD_GUI_TXT_TIP			"Left click to bind a key\nRight click to unbind"
#define SMD_GUI_TXT_SMD			"Mouselook"
#define SMD_GUI_TXT_SMD_ACT		SMD_GUI_TXT_SMD" (Active)"
#define SMD_GUI_TXT_SMD_TST		SMD_GUI_TXT_SMD" (Test):"

#define SMD_GUI_IN_RECT(x, y, r) ((x>=(r)->left)&&(x<(r)->right)&&(y>=(r)->top)&&(y<(r)->bottom))

enum {
	SMD_GUI_MINX_MIN,
	SMD_GUI_MINX_X,
	SMD_GUI_SYS_CNT,
};
