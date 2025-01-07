/**
 * FileName: access.c
 *
 * Copyright (C) 2011-2012, ZKSoftware Inc.
 *
 * Created: 2012-7-27
 * Author: dsl
 *
 * Description: 门禁管理窗口
 */
#include <stdio.h>
#include <string.h>

#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>

#include "zktype.h"
#include "printlog.h"
#include "hubclient.h"
#include "zkconstant.h"

#include "appinfo.h"
#include "busywin.h"
#include "menupublic.h"
#include "miniguiclient.h"
#include "languageres.h"
#include "access.h"
#include "permission.h"
#include "flashdb.h"
#include "msg.h"
#include "libverify.h"
#include "appcommon.h"
#include "miniguiclient.h"
#include "keyboardtype.h"
#include "maincommucmd.h"

#include "strutils.h"
#include "stylecfg.h"
#include "language.h"
#include "option.h"
#include "oplog.h"
#include "loadaccesslib.h"
#include "libappcommonapi.h"
#include "accessproxy.h"
#include "maincommucmd.h"

#define IDC_ACCESSLIST 100				/*列表控件ID*/

static HWND ghAccessListView;	/*列表控件句柄*/
static HDC gAccessHdc;		/*门管理窗口DC*/

static BITMAP gTimezoneBmp;	/*时间段位图*/
static BITMAP gHolidayBmp;	/*节假日位图*/
static BITMAP gGroupBmp;	/*门禁组位图*/
static BITMAP gUnlockCombBmp;	/*开锁组合位图*/
static BITMAP gACParamBmp;	/*门禁参数位图*/
static BITMAP gDruessBmp;	/*胁迫位图*/
static BITMAP gAntiBmp;		/*反潜设置位图*/

static TAppPreProcData gAppPreProcData; /*应用程序预处理数据*/

#define BIT_TZ		1
#define BIT_HOLIDAY	2
#define BIT_GROUP	4
#define BIT_COMB	8
#define BIT_PARAM	16
#define BIT_DURESS	32

int GetAccessExecPermission(void)
{
	return gAppPreProcData.curUserPermission;
}

void ResetAccessExecPermission(int permission)
{
	gAppPreProcData.curUserPermission = permission;
}

/**
 * 函数名	: LoadAccessMngBmp
 * 功能说明	: 加载门禁管理图片资源
 * 参数说明	:
 * 		无
 * 返回值	:
 * 		0 	: 加载成功
 * 		>0 	: 加载失败
 */
static int LoadAccessMngBmp(void)
{
	int err = 0;

	if (LoadBitmap(HDC_SCREEN, &gACParamBmp, (char*)GetAppBmpPath("parameters.png")))
	{
		err++;
	}

	if(GetIntOption(OPT_LOCKFUNON) > 1)//以下功能简单门禁不需要
	{
		if (LoadBitmap(HDC_SCREEN, &gTimezoneBmp, (char*)GetAppBmpPath("timezone.png")))
		{
			err++;
		}
		if (LoadBitmap(HDC_SCREEN, &gHolidayBmp, (char*)GetAppBmpPath("holiday.png")))
		{
			err++;
		}
		if (LoadBitmap(HDC_SCREEN, &gGroupBmp, (char*)GetAppBmpPath("group.png")))
		{
			err++;
		}
		if (LoadBitmap(HDC_SCREEN, &gUnlockCombBmp, (char*)GetAppBmpPath("unlockcomb.png")))
		{
			err++;
		}
		if (LoadBitmap(HDC_SCREEN, &gDruessBmp, (char*)GetAppBmpPath("duressalarm.png")))
		{
			err++;
		}
		if (LoadBitmap(HDC_SCREEN, &gAntiBmp, (char*)GetAppBmpPath("AntiPassback.png")))
		{
			err++;
		}
	}
	if (err)
	{
		PrintWarn("Fail to load usermng bitmap.\n");
	}

	return err;
}

/**
 * 函数名	: UnloadAccessBmp
 * 功能说明	: 释放门禁管理图片资源
 * 参数说明	:
 * 		无
 * 返回值	:
 * 		无
 */
static void UnloadAccessBmp(void)
{
	UnloadBitmap(&gTimezoneBmp);
	UnloadBitmap(&gHolidayBmp);
	UnloadBitmap(&gGroupBmp);

	UnloadBitmap(&gUnlockCombBmp);
	UnloadBitmap(&gACParamBmp);

	if(0==GetIntOption(OPT_ACCESSRULETYPE))
	{
		UnloadBitmap(&gDruessBmp);
	}

	UnloadBitmap(&gAntiBmp);
}

static int ExecShortcutKeyFuncProc(char *pShortcutKeyFuncName)
{
	int ret = 0;

	if (NULL != pShortcutKeyFuncName && 0 != pShortcutKeyFuncName[0])
	{
		/*		if(0 == strcmp(pShortcutKeyFuncName, FUNC_NAME_ACTIMEZONE))
		 {
		 ret = CreateNewUserWindow(HWND_DESKTOP);
		 }
		 else if(0 == strcmp(pShortcutKeyFuncName, FUNC_NAME_ACUNLOCKCOMB))
		 {
		 ret = CreateUserListWindow(HWND_DESKTOP);
		 }*/
	}

	return ret;
}

/**
 * 函数名	: InitAccessWindow
 * 功能说明	: 初始化门禁管理窗口
 * 参数说明	:
 * 		hWnd
 * 			[in] 主窗口句柄
 * 		hdc
 * 			[in] 主窗口DC
 * 返回值	:
 * 		无
 */
static void InitAccessWindow(HWND hWnd, HDC hdc)
{
	int x = 0;
	int y = GetBarHeight();
	int w = g_rcScr.right;
	int h = g_rcScr.bottom - GetBarHeight();
	int nItem = 0;
	int dispFlag = BIT_TZ | BIT_HOLIDAY | BIT_GROUP | BIT_COMB | BIT_PARAM | BIT_DURESS;

	if (1 == GetIntOption(OPT_LOCKFUNON))
	{
		dispFlag = BIT_PARAM;
	}

	InitWindowCaption(hWnd, hdc, (char*)LoadStrByID(LANG_APP_NAME));

	ghAccessListView = CreateCustomListView(hWnd, IDC_ACCESSLIST, x, y, w, h, SECOND_MENU_LISTVIEW, NULL);

	PAccessProxy p = GetAccessProxy();

	/*门禁管理参数*/
	if ((BIT_PARAM & dispFlag) && CheckFuncPermission(FUNC_NAME_ACPARAM, gAppPreProcData.curUserPermission))
	{
		AddSecondMenuItem(ghAccessListView, nItem++, (char*)LoadStrByID(LANG_ACC_PARAM), &gACParamBmp,
				(MENUBACKCALL)p->CreateAccessSettingWindow);
	}
	
	if(CLOUD_TYPE_OVERSEAS != GetIntOption(OPT_PROCLOUDTYPE))
	{
		/*时间段设置*/
		if ((BIT_TZ & dispFlag) && CheckFuncPermission(FUNC_NAME_ACTIMEZONE, gAppPreProcData.curUserPermission))
		{
			if (GetIntOption(OPT_ACCESSRULETYPE) )
			{
				AddSecondMenuItem(ghAccessListView, nItem++, (char*)LoadStrByID(LANG_TIMERULE_SETTING), &gTimezoneBmp,
						(MENUBACKCALL)p->CreateTimeZoneSettingWindow);
			}
			else
			{
				AddSecondMenuItem(ghAccessListView, nItem++, (char*)LoadStrByID(LANG_TIMEZONE_SETTING), &gTimezoneBmp,
					(MENUBACKCALL)p->CreateTimeZoneSettingWindow);
			}

	}

	/*节假日设置*/
	if ((BIT_HOLIDAY & dispFlag) && CheckFuncPermission(FUNC_NAME_ACHOLIDAY, gAppPreProcData.curUserPermission))
	{
		AddSecondMenuItem(ghAccessListView, nItem++, (char*)LoadStrByID(LANG_HOLIDAY_SETTING), &gHolidayBmp,
				(MENUBACKCALL)p->CreateHolidySettingWindow);
	}

	if (GetIntOption(OPT_ACCESSRULETYPE) == 0)
	{
		/*门禁组设置*/
		if ((BIT_GROUP & dispFlag) && CheckFuncPermission(FUNC_NAME_ACGROUP, gAppPreProcData.curUserPermission))
		{
			AddSecondMenuItem(ghAccessListView, nItem++, (char*)LoadStrByID(LANG_GROUP_SETTING), &gGroupBmp,
					(MENUBACKCALL)p->CreateAccessGroupSettingWindow);
		}
	}

	/*开锁组合设置*/
	if (GetIntOption(OPT_DOOR1MULTICARDOPENDOORFUNON) && (BIT_COMB & dispFlag) && CheckFuncPermission(FUNC_NAME_ACUNLOCKCOMB, gAppPreProcData.curUserPermission))
	{
		AddSecondMenuItem(ghAccessListView, nItem++, (char*)LoadStrByID(LANG_UNLOCKCOMB), &gUnlockCombBmp,
				(MENUBACKCALL)p->CreateUnlockCombineSettignWindow);
	}

	/*反潜设置*/
	if ((BIT_PARAM & dispFlag) && CheckFuncPermission(FUNC_NAME_ANTIPASSBACKSET, gAppPreProcData.curUserPermission)
			&& GetIntOption(OPT_ANTIPASSBACKFUNON))
	{
		AddSecondMenuItem(ghAccessListView, nItem++, (char*)LoadStrByID(LANG_ANTI_PASSBACK_SET), &gAntiBmp,
				(MENUBACKCALL)p->CreateAntiPassBackSettingWindow);
	}

	/*胁迫报警参数*/
	if (0==GetIntOption(OPT_ACCESSRULETYPE) && (BIT_DURESS & dispFlag) \
			&& CheckFuncPermission(FUNC_NAME_ACDURESSALARM, gAppPreProcData.curUserPermission))
	{
		AddSecondMenuItem(ghAccessListView, nItem++, (char*)LoadStrByID(LANG_ALARM_PARAM), &gDruessBmp,
				(MENUBACKCALL)p->CreateDuressSettingWindow);
	}

		/*胁迫报警参数*/
		if (1==GetIntOption(OPT_ACCESSRULETYPE) && (BIT_DURESS & dispFlag) \
				&& CheckFuncPermission(FUNC_NAME_ACDURESSALARM, gAppPreProcData.curUserPermission))
		{
			AddSecondMenuItem(ghAccessListView, nItem++, (char*)LoadStrByID(LANG_ALARM_PARAM), &gDruessBmp,
					(MENUBACKCALL)p->CreateDuressSettingWindow);
		}
	}
/*
	AddSecondMenuItem(ghAccessListView, nItem++, "Turn On Alarm", &gDruessBmp,
			(MENUBACKCALL)AccessTesingWindow);
	AddSecondMenuItem(ghAccessListView, nItem++, "Turn Off Alarm", &gDruessBmp,
			(MENUBACKCALL)AccessTesingWindow2);
	AddSecondMenuItem(ghAccessListView, nItem++, "Unlock", &gDruessBmp,
			(MENUBACKCALL)AccessTesingWindow3);
	AddSecondMenuItem(ghAccessListView, nItem++, "Still Lock", &gDruessBmp,
			(MENUBACKCALL)AccessTesingWindow4);
	AddSecondMenuItem(ghAccessListView, nItem++, "Still Unlock", &gDruessBmp,
			(MENUBACKCALL)AccessTesingWindow5);
	AddSecondMenuItem(ghAccessListView, nItem++, "restore lock default state", &gDruessBmp,
			(MENUBACKCALL)AccessTesingWindow6);
	AddSecondMenuItem(ghAccessListView, nItem++, "Set Sensor Mode", &gDruessBmp,
			(MENUBACKCALL)AccessTesingWindow7);
*/

	SetFocusChild(ghAccessListView);
	SendMessage(ghAccessListView, LVM_CHOOSEITEM, 0, 0);

	if(IsSupportTuchScreen())
	{
		CustomLVShowPageUpDownBtn(hWnd, ghAccessListView);
	}

	return;
}

/**
 * 函数名	: AccessWinProc
 * 功能说明	: 门禁管理窗口过程
 * 参数说明	:
 * 		hWnd
 * 			[in] 接收消息的窗口句柄
 * 		message
 * 			[in] 消息标识符
 * 		wParam
 * 			[in] 参数值
 * 		lParam
 * 			[in] 参数值
 */
static int AccessWinProc(HWND hWnd, int message, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;

	switch (message)
	{
		case MSG_CREATE:
		{
			ClientAppRequest(REQ_HIDE_MENU, 0, 1);
			InitAccessProxy(GetIntOption(OPT_ACCESSRULETYPE));
			InitAccessWindow(hWnd, gAccessHdc);
			UpdateWindow(hWnd, TRUE);
			break;
		}
		case MSG_PAINT:
		{
			hdc = BeginPaint(hWnd);
			EndPaint(hWnd, hdc);
			return 0;
		}
		case MSG_ERASEBKGND:
		{
			HDC hdc = (HDC)wParam;
			const RECT* clip = (const RECT*)lParam;
			BOOL fGetDC = FALSE;
			RECT rcTemp;

			if (hdc == 0)
			{
				hdc = GetSecondaryClientDC(hWnd);
				fGetDC = TRUE;
			}
			if (clip)
			{
				rcTemp = *clip;
				ScreenToClient(hWnd, &rcTemp.left, &rcTemp.top);
				ScreenToClient(hWnd, &rcTemp.right, &rcTemp.bottom);
				IncludeClipRect(hdc, &rcTemp);
			}

			BitBlt(gAccessHdc, 0, 0, 0, 0, hdc, 0, 0, 0);

			if (fGetDC)
			{
				ReleaseSecondaryDC(hWnd, hdc);
			}
			return 0;
		}

		case MSG_KEYDOWN:
		{
			if(wParam == ZK_INPUT_DOORSENSOR)
			{
				PrintDebug("access get zkinputsensor message  send it to mainwindow");
				PostData(HUB_CLIENT_SOCKET_NAME_MAIN, 1, MAIN_CMD_UPDATA_MESSAGE, MSG_KEYDOWN, &wParam);
			}
			switch(HookTouchScreenPageUPDownMsg2OperateListView(ghAccessListView, wParam))
			{
				case LV_ALREADY_AT_EHD:
				{
					SendMessage(ghAccessListView, LVM_CHOOSEITEM, 0, 0);
					return TRUE;
				}
				case LV_ALREADY_AT_BEGIN:
				case LV_OPREAD_PAGE_UP_DOWN_SUCCESS:
					return TRUE;

				case LV_OPREAD_DO_NOTHING:
				default:
					break;
			}

			if (LOWORD(wParam) == SCANCODE_ESCAPE)
			{
				PostMessage(hWnd, MSG_CLOSE, 0, 0L);
			}
			if (wParam == SCANCODE_CURSORBLOCKDOWN)
			{
				SelectNextLVItem(ghAccessListView, 1, 1);
			}
			if (wParam == SCANCODE_CURSORBLOCKUP)
			{
				SelectNextLVItem(ghAccessListView, 0, 1);
			}
			if (SCANCODE_ENTER == wParam || CheckKeyAsReuseKey(wParam, SCANCODE_ENTER))
			{
				int nItem = SendMessage(ghAccessListView, SVM_GETCURSEL, 0, 0);
				IMG_LISTVIEW_DATA_PT lvItem = (IMG_LISTVIEW_DATA_PT)SendMessage(ghAccessListView,
						LVM_GETITEMADDDATA, nItem, 0);

				if ((MENUBACKCALL)lvItem->Proc)
				{
					lvItem->Proc(hWnd);
				}
			}
			return 0;
		}
		case MSG_CLOSE:
		{
			ClientAppRequest(REQ_SHOW_MENU, 0, 1);
			DestroyMainWindow(hWnd);
			FreeAccessProxy();
			DeleteMemDC(gAccessHdc);
			return 0;
		}
	}

	return DefaultMainWinProc(hWnd, message, wParam, lParam);
}

int MiniGUIMain(int argc, const char* argv[])
{
	MSG Msg;
	HWND hMainWnd;
	MAINWINCREATE CreateInfo;
	int ret = -1;
	TUser operator;
	
#ifdef _MGRM_PROCESSES
	JoinLayer(NAME_DEF_LAYER , APP_NAME_ACCESS, 0 , 0);
#endif
	ShowBusyWindow(TRUE);
	SetPrintLogType(argc, argv, HUB_CLIENT_SOCKET_NAME_ACCESS);
	AppCommonInitProc(HUB_CLIENT_SOCKET_NAME_ACCESS);
	AppCommonApiInit(HUB_CLIENT_SOCKET_NAME_ACCESS);
	memset(&gAppPreProcData, 0, sizeof(TAppPreProcData));
	gAppPreProcData.pAppName = APP_NAME_ACCESS;
	gAppPreProcData.ExecShortcutKeyFunc = ExecShortcutKeyFuncProc;
	SetAppPreProcDataInfo(&gAppPreProcData);

	BOOL appContinueFlag = AppPreProcess(argc, argv);

	if (!appContinueFlag)
	{
		AppCommonEndProc();
		return 0;
	}

	if(GetIntOption(OPT_USEHWFIRMWARE))
	{		
		ret = GetOperatorFromDB(&operator);
		if(ret != FALSE)
		{
			PrintDebug("operator.User_PIN=%s",operator.User_PIN);
			SetOperator(operator.User_PIN);
		}
	}
	else
	{
		SetOperator(gAppPreProcData.pCurUserPin);
	}

	LoadAccessMngBmp();

#if 0
	/* 这里主要是门禁的参数设置之后需要实时的更新，修改为由main去更新，这里不在load Access */
	//初始化高级门禁库
	if(!GetIntOption(OPT_ACCESSRULETYPE) && (GetIntOption(OPT_LOCKFUNON) > 1))
	{
		LoadAccessLib();
	}
#endif

	gAccessHdc = CreateCompatibleDC(HDC_SCREEN);

	CreateInfo.dwStyle = WS_VISIBLE;
	CreateInfo.dwExStyle = WS_EX_AUTOSECONDARYDC;
	CreateInfo.spCaption = "";
	CreateInfo.hMenu = 0;
	CreateInfo.hCursor = GetSystemCursor(0);
	CreateInfo.hIcon = 0;
	CreateInfo.MainWindowProc = AccessWinProc;
	CreateInfo.lx = 0;
	CreateInfo.ty = 0;
	CreateInfo.rx = g_rcScr.right;
	CreateInfo.by = g_rcScr.bottom;
	CreateInfo.iBkColor = COLOR_black;
	CreateInfo.dwAddData = 0;
	CreateInfo.hHosting = HWND_DESKTOP;

	hMainWnd = CreateMainWindow(&CreateInfo);
	if (hMainWnd == HWND_INVALID)
	{
		UnloadAccessBmp();
		FreeAccessLib();
		DeleteMemDC(gAccessHdc);
		ShowBusyWindow(FALSE);

		return -1;
	}

	ShowWindow(hMainWnd, SW_SHOWNORMAL);
	ShowBusyWindow(FALSE);
	while (GetMessage(&Msg, hMainWnd))
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	AppCommonEndProc();
	AppCommonEndProcEx();
	UnloadAccessBmp();

#if 0
	FreeAccessLib();
#endif

	MainWindowThreadCleanup(hMainWnd);
	hMainWnd = HWND_INVALID;

	return 0;
}

#ifdef _MGRM_THREADS
#include <minigui/dti.c>
#endif
