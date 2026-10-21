/* S60 3rd Edition (Symbian 9.2 / 9.3) shell for the oe-re / oe2-re engines.
 * Replaces src/main.c. The game loop matches the desktop port: 15 ms ticks,
 * key repeat after 1500 ms every 100 ms, suspend on background. */

#include "oe_app.h"

#include <eikstart.h>
#include <aknnotewrappers.h>
#include <eikspane.h>
#include <eikbtgpc.h>
#include <e32std.h>
#include <stdio.h>
extern "C" int snprintf(char* s, size_t n, const char* fmt, ...);

/* Same stdint bypass as platform_s60.cpp: Open C's header breaks under C++. */
#ifndef _STDINT_H_
#define _STDINT_H_
#define _STDINT_H
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
typedef uint32_t uintptr_t;
typedef int32_t intptr_t;
#endif

extern "C" {
#include "platform.h"
void OeS60_SetPresentTarget(TUint16* aPixels, int aPitch, int aPhoneW, int aPhoneH);
void OeS60_PushKey(int aAvk, int aDown);
void OeS60_SetMinimized(int aMinimized);
void OeS60_RequestQuit(void);

/* engine entry points (app.c / brew_shim.c) */
int Brew_init(const char* data, int w, int h);
void Brew_shutdown(void);
struct Applet;
struct Applet* App_create(int w, int h);
void App_destroy(struct Applet* app);
int Applet_HandleEvent(struct Applet* app, int evt, int wParam, int dwParam);
void Applet_Tick(struct Applet* app);
}

static const int KEngW = 176;
static const int KEngH = 220;
static const TUint KRepeatStart = 1500;
static const TUint KRepeatRate = 100;
static const int KKbAutorepeat = 0x40;

static struct Applet* gApp = 0;
static int gHeldKey = 0;
static TUint32 gNextRepeat = 0;

COeDocument::COeDocument(CEikApplication& aApp) : CAknDocument(aApp) {}

COeDocument* COeDocument::NewL(CEikApplication& aApp)
{
	COeDocument* self = new (ELeave) COeDocument(aApp);
	return self;
}

static void BootLog(const char* aMsg);

CEikAppUi* COeDocument::CreateAppUiL()
{
	BootLog("b3 CreateAppUiL");
	return new (ELeave) COeAppUi;
}

static void BootLogNum(const char* aTag, TInt aNum)
{
	char b[64];
	snprintf(b, sizeof(b), "%s %d", aTag, (int)aNum);
	BootLog(b);
}

static void ProbeFile(RFs& aFs, const TDesC& aPath, const char* aTag)
{
	TEntry e;
	TInt err = aFs.Entry(aPath, e);
	char b[64];
	if (err == KErrNone)
		snprintf(b, sizeof(b), "%s: found, %d bytes", aTag, (int)e.iSize);
	else
		snprintf(b, sizeof(b), "%s: err %d", aTag, (int)err);
	BootLog(b);
}

void COeApplication::PreDocConstructL()
{
	BootLog("b5 PreDoc");
	RFs fs;
	if (fs.Connect() == KErrNone) {
		ProbeFile(fs, _L("c:\\resource\\apps\\oe2.rsc"), "c: oe2.rsc");
		ProbeFile(fs, _L("e:\\resource\\apps\\oe2.rsc"), "e: oe2.rsc");
		ProbeFile(fs, _L("c:\\private\\10003a3f\\import\\apps\\oe2_reg.rsc"), "c: oe2_reg.rsc");
		ProbeFile(fs, _L("e:\\private\\10003a3f\\import\\apps\\oe2_reg.rsc"), "e: oe2_reg.rsc");
		fs.Close();
	}
#ifdef OE_NO_APP_RSC
	/* Isolation / Open C style: do not let the framework load oe2.rsc at all. */
	BootLog("PreDoc skipped (OE_NO_APP_RSC)");
	return;
#else
	BootLog("before CAknApplication::PreDocConstructL");
	/* Do NOT skip CAknApplication: it does the Avkon setup. Calling
	 * CEikApplication::PreDocConstructL() directly bypassed it. If the SDK's
	 * CAknApplication does not declare its own version, this resolves to the
	 * inherited CEikApplication one, so it compiles either way. */
	TRAPD(err, CAknApplication::PreDocConstructL());
	BootLogNum("PreDocConstructL result", err);
	User::LeaveIfError(err);
#endif
}

CApaDocument* COeApplication::CreateDocumentL()
{
	return COeDocument::NewL(*this);
}

static void BootLog(const char* aMsg);

EXPORT_C CApaApplication* NewApplication()
{
	BootLog("b3 NewApplication");
	return new COeApplication;
}

GLDEF_C TInt E32Main()
{
	BootLog("b3 E32Main");
	return EikStart::RunApplication(NewApplication);
}

static TBool DataDirUsable(const TDesC& aDir)
{
	RFs fs;
	if (fs.Connect() != KErrNone)
		return EFalse;
	TEntry e;
	TFileName bar = aDir;
#if OE_GAME == 2
	bar.Append(_L("orcsandelves2.bar"));
#else
	bar.Append(_L("orcsandelves.bar"));
#endif
	TInt err = fs.Entry(bar, e);
	fs.Close();
	return err == KErrNone && e.iSize > 0;
}

/* Open C fopen() on S60 wants one kind of slash. Brew_init appends
 * "/orcsandelves2.bar", so the directory must use forward slashes too,
 * otherwise the path becomes e:\data\oe2\/orcsandelves2.bar and fopen fails
 * even though RFs::Entry found the file. */
static void PickDataDir(char* aOut, int aCap)
{
	const TPtrC dirs[] = {
		_L("e:\\data\\oe\\"),
		_L("c:\\data\\oe\\"),
		_L("e:\\data\\oe2\\"),
		_L("c:\\data\\oe2\\")
	};
	for (TUint i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
		if (!DataDirUsable(dirs[i]))
			continue;
		TBuf8<256> u;
		u.Copy(dirs[i]);
		int n = u.Length();
		if (n >= aCap)
			n = aCap - 1;
		for (int k = 0; k < n; ++k)
			aOut[k] = (u[k] == '\\') ? '/' : (char)u[k];
		aOut[n] = 0;
		return;
	}
#if OE_GAME == 2
	const char* fb = "e:/data/oe2/";
#else
	const char* fb = "e:/data/oe/";
#endif
	int i = 0;
	for (; fb[i] && i + 1 < aCap; ++i)
		aOut[i] = fb[i];
	aOut[i] = 0;
}

static void ShowFailL(const TDesC& aMsg)
{
	CAknInformationNote* note = new (ELeave) CAknInformationNote;
	note->ExecuteLD(aMsg);
}

/* Survives a panic: written with RFs, not Open C, so a crash still leaves a trail. */
static void BootLog(const char* aMsg)
{
	const TPtrC dirs[] = { _L("e:\\data\\oe2\\"), _L("c:\\data\\oe2\\"), _L("e:\\data\\oe\\"), _L("c:\\data\\oe\\") };
	RFs fs;
	if (fs.Connect() != KErrNone)
		return;
	for (TUint i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
		TFileName path = dirs[i];
		path.Append(_L("oe2boot.txt"));
		RFile f;
		TInt err = f.Open(fs, path, EFileWrite);
		if (err == KErrNotFound)
			err = f.Create(fs, path, EFileWrite);
		if (err != KErrNone)
			continue;
		TInt size = 0;
		f.Size(size);
		f.Seek(ESeekStart, size);
		TBuf8<160> line;
		TInt ml = User::StringLength((const TUint8*)aMsg);
		if (ml > 150)
			ml = 150;
		line.Copy(TPtrC8((const TUint8*)aMsg, ml));
		line.Append(_L8("\r\n"));
		f.Write(line);
		f.Close();
		break;
	}
	fs.Close();
}

void COeAppUi::HideFurniture()
{
	CEikStatusPane* sp = StatusPane();
	if (sp)
		sp->MakeVisible(EFalse);
	CEikButtonGroupContainer* cba = Cba();
	if (cba)
		cba->MakeVisible(EFalse);
}

void COeAppUi::ConstructL()
{
	BootLog("b2 start");
#ifdef OE_NO_APP_RSC
	BaseConstructL(CAknAppUi::EAknEnableSkin | ENoAppResourceFile);
#else
	BaseConstructL(CAknAppUi::EAknEnableSkin);
#endif
	BootLog("b2 BaseConstructL ok");
	HideFurniture();
	/* Full screen: ApplicationRect() includes the area the status pane and
	 * softkeys would otherwise take. ClientRect() left a bar at the top. */
	iContainer = COeContainer::NewL(ApplicationRect());
	AddToStackL(iContainer);
	BootLog("container ok");

	char dir[256];
	PickDataDir(dir, (int)sizeof(dir));
	Plat_setDataDir(dir);
	BootLog(dir);
	{
		char ep[300];
		snprintf(ep, sizeof(ep), "%soe2err.txt", dir);
		freopen(ep, "a", stderr);
		setvbuf(stderr, NULL, _IONBF, 0);
	}

	if (Brew_init(dir, KEngW, KEngH) != 0) {
		BootLog("brew_init failed");
		TBuf<160> msg;
		msg.Copy(TPtrC8((const TUint8*)dir));
		msg.Append(_L(" open failed"));
		ShowFailL(msg);
		Exit();
		return;
	}
	BootLog("brew_init ok");
	if (Plat_init(KEngW, KEngH, "Orcs & Elves") != 0) {
		BootLog("plat_init failed");
		ShowFailL(_L("Plat_init failed"));
		Brew_shutdown();
		Exit();
		return;
	}
	Audio_init();
	iContainer->SetEngineSize(KEngW, KEngH);
	/* The FBS heap is locked only while the engine writes into it (see Tick). */
	iContainer->Lock();
	OeS60_SetPresentTarget(iContainer->Pixels(), iContainer->PitchPixels(), iContainer->Width(), iContainer->Height());

	gApp = App_create(KEngW, KEngH);
	iContainer->Unlock();
	if (!gApp) {
		BootLog("app_create failed");
		ShowFailL(_L("App_create failed"));
		Plat_shutdown();
		Brew_shutdown();
		Exit();
		return;
	}
	BootLog("before app_start");
	iContainer->Lock();
	OeS60_SetPresentTarget(iContainer->Pixels(), iContainer->PitchPixels(), iContainer->Width(), iContainer->Height());
	Applet_HandleEvent(gApp, EVT_APP_START, 0, 0);
	iContainer->Unlock();
	BootLog("app_start ok");

	iTicker = CPeriodic::NewL(CActive::EPriorityStandard);
	/* 40 ms, the phone's original ISHELL timer (Applet_Tick rearms it). */
	iTicker->Start(40 * 1000, 40 * 1000, TCallBack(&COeAppUi::TickCb, this));
	BootLog("ticker ok");
}

COeAppUi::~COeAppUi()
{
	if (iTicker) {
		iTicker->Cancel();
		delete iTicker;
	}
	if (gApp) {
		Applet_HandleEvent(gApp, EVT_APP_STOP, 0, 0);
		App_destroy(gApp);
		gApp = 0;
	}
	Plat_shutdown();
	Brew_shutdown();
	if (iContainer) {
		RemoveFromStack(iContainer);
		delete iContainer;
	}
}

void COeAppUi::HandleCommandL(TInt aCommand)
{
	if (aCommand == EEikCmdExit || aCommand == EAknSoftkeyExit)
		Exit();
}

void COeAppUi::HandleForegroundEventL(TBool aForeground)
{
	CAknAppUi::HandleForegroundEventL(aForeground);
	if (aForeground)
		HideFurniture();
	int suspended = aForeground ? 0 : 1;
	OeS60_SetMinimized(suspended);
	if (gApp) {
		gHeldKey = 0;
		Applet_HandleEvent(gApp, suspended ? EVT_APP_SUSPEND : EVT_APP_RESUME, 0, 0);
		Audio_pauseAll(suspended);
	}
}

TInt COeAppUi::TickCb(TAny* aSelf)
{
	static_cast<COeAppUi*>(aSelf)->Tick();
	return KErrNone;
}

void COeAppUi::Tick()
{
	static int once = 0;
	if (!once) {
		once = 1;
		BootLog("first tick");
	}
	if (!gApp)
		return;
	PlatKeyEvent ev[16];
	int n = 0;
	if (!Plat_pollEvents(ev, 16, &n)) {
		Exit();
		return;
	}
	for (int i = 0; i < n; ++i) {
		if (ev[i].repeat)
			continue;
		if (ev[i].down) {
			Applet_HandleEvent(gApp, EVT_KEY, ev[i].avk, 0);
			gHeldKey = ev[i].avk;
			gNextRepeat = Plat_uptimeMs() + KRepeatStart;
		} else if (ev[i].avk == gHeldKey) {
			gHeldKey = 0;
		}
	}
	if (gHeldKey && (TInt32)(Plat_uptimeMs() - gNextRepeat) >= 0) {
		Applet_HandleEvent(gApp, EVT_KEY, gHeldKey, KKbAutorepeat);
		gNextRepeat += KRepeatRate;
	}
	if (iContainer) {
		/* lock only for the engine tick; refresh the pointer after every lock */
		iContainer->Lock();
		OeS60_SetPresentTarget(iContainer->Pixels(), iContainer->PitchPixels(), iContainer->Width(), iContainer->Height());
	}
	Applet_Tick(gApp);
	if (iContainer) {
		OeS60_SetPresentTarget(0, 0, iContainer->Width(), iContainer->Height()); /* nothing may touch it unlocked */
		iContainer->Unlock();
		iContainer->DrawNow();
	}
}

/* Joystick, softkeys and keypad of an S60 3rd phone (N80), translated to the
 * BREW codes the engine already understands (same mapping as the desktop port). */
TKeyResponse COeAppUi::HandleKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
{
	if (aType != EEventKeyDown && aType != EEventKeyUp && aType != EEventKey)
		return EKeyWasNotConsumed;
	/* EEventKey is the character event; the down/up pair is enough and
	 * consuming both would double every press. */
	if (aType == EEventKey)
		return EKeyWasConsumed;

	int avk = 0;
	switch (aKeyEvent.iScanCode) {
	case EStdKeyUpArrow:    avk = AVK_UP; break;
	case EStdKeyDownArrow:  avk = AVK_DOWN; break;
	case EStdKeyLeftArrow:  avk = AVK_LEFT; break;
	case EStdKeyRightArrow: avk = AVK_RIGHT; break;
	case EStdKeyDevice3:    avk = AVK_SELECT; break; /* joystick press */
	case EStdKeyDevice0:    avk = AVK_SOFT1; break;  /* left softkey: menu */
	case EStdKeyDevice1:    avk = AVK_SOFT2; break;
	case EStdKeyNkpAsterisk:
	case EStdKeyIncVolume:  avk = AVK_STAR; break;
	case EStdKeyHash:
	case EStdKeyDecVolume:  avk = AVK_POUND; break;
	case EStdKeyBackspace:  avk = AVK_CLR; break;
	case EStdKeyYes:        avk = AVK_SEND; break;
	case EStdKeyNo:
		OeS60_RequestQuit();
		avk = AVK_END;
		break;
	case EStdKeyNkp0: case '0': avk = AVK_0; break;
	case EStdKeyNkp1: case '1': avk = AVK_1; break;
	case EStdKeyNkp2: case '2': avk = AVK_2; break;
	case EStdKeyNkp3: case '3': avk = AVK_3; break;
	case EStdKeyNkp4: case '4': avk = AVK_4; break;
	case EStdKeyNkp5: case '5': avk = AVK_5; break;
	case EStdKeyNkp6: case '6': avk = AVK_6; break;
	case EStdKeyNkp7: case '7': avk = AVK_7; break;
	case EStdKeyNkp8: case '8': avk = AVK_8; break;
	case EStdKeyNkp9: case '9': avk = AVK_9; break;
	default:
		return EKeyWasNotConsumed;
	}
	OeS60_PushKey(avk, aType == EEventKeyDown ? 1 : 0);
	return EKeyWasConsumed;
}

COeContainer* COeContainer::NewL(const TRect& aRect)
{
	COeContainer* self = new (ELeave) COeContainer;
	CleanupStack::PushL(self);
	self->ConstructL(aRect);
	CleanupStack::Pop(self);
	return self;
}

void COeContainer::ConstructL(const TRect& aRect)
{
	CreateWindowL();
	iBitmap = new (ELeave) CFbsBitmap;
	/* Back buffer = the real screen size, so the same build runs on 352x416 (N80),
	 * 240x320 (N95, E65...) and others. The engine output is scaled into it. */
	iW = aRect.Width();
	iH = aRect.Height();
	if (iW < 64 || iH < 64) { iW = 240; iH = 320; }
	User::LeaveIfError(iBitmap->Create(TSize(iW, iH), EColor64K));
	iPitch = iBitmap->ScanLineLength(iW, EColor64K) / 2;
	iDevice = CFbsBitmapDevice::NewL(iBitmap);
	User::LeaveIfError(iDevice->CreateContext(iGc));
	SetRect(aRect);
	ActivateL();
}

COeContainer::~COeContainer()
{
	delete iGc;
	delete iDevice;
	delete iBitmap;
}

void COeContainer::SetEngineSize(TInt aW, TInt aH)
{
	iEngW = aW;
	iEngH = aH;
}

TUint16* COeContainer::Pixels()
{
	if (!iBitmap)
		return 0;
	return (TUint16*)iBitmap->DataAddress();
}

void COeContainer::Lock()
{
	if (iBitmap)
		iBitmap->LockHeap();
}

void COeContainer::Unlock()
{
	if (iBitmap)
		iBitmap->UnlockHeap();
}

void COeContainer::Draw(const TRect& /*aRect*/) const
{
	if (!iBitmap)
		return;
	CWindowGc& gc = SystemGc();
	gc.BitBlt(Rect().iTl, iBitmap);
}
