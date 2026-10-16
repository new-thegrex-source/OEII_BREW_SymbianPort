#ifndef OE_APP_H
#define OE_APP_H

#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <coecntrl.h>

/* Built twice. oe.mmp passes -DOE_GAME=1, oe2.mmp passes -DOE_GAME=2. */
#ifndef OE_GAME
#define OE_GAME 1
#endif

#if OE_GAME == 2
static const TUid KUidOeApp = { 0xE0E2E002 };
_LIT(KOeExeName, "oe2");
_LIT(KOeCaption, "Orcs & Elves II");
#else
static const TUid KUidOeApp = { 0xE0E1E001 };
_LIT(KOeExeName, "oe");
_LIT(KOeCaption, "Orcs & Elves");
#endif

class COeAppUi;

/* Phone-side framebuffer the engine scales into. 352x416 is the N80 panel. */
class COeContainer : public CCoeControl
{
public:
	static COeContainer* NewL(const TRect& aRect);
	~COeContainer();
	void Draw(const TRect& aRect) const;
	TUint16* Pixels();
	void Lock();
	void Unlock();
	TInt PitchPixels() const { return iPitch; }
	void SetEngineSize(TInt aW, TInt aH);
private:
	void ConstructL(const TRect& aRect);
	CFbsBitmap* iBitmap;
	CFbsBitmapDevice* iDevice;
	CFbsBitGc* iGc;
	TInt iPitch;
	TInt iEngW;
	TInt iEngH;
};

class COeDocument : public CAknDocument
{
public:
	static COeDocument* NewL(CEikApplication& aApp);
	CEikAppUi* CreateAppUiL();
private:
	COeDocument(CEikApplication& aApp);
};

class COeApplication : public CAknApplication
{
public:
	TUid AppDllUid() const { return KUidOeApp; }
protected:
	CApaDocument* CreateDocumentL();
	void PreDocConstructL();
};

class COeAppUi : public CAknAppUi
{
public:
	void ConstructL();
	~COeAppUi();
	void HandleCommandL(TInt aCommand);
	void HandleForegroundEventL(TBool aForeground);
	TKeyResponse HandleKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
	COeContainer* Container() { return iContainer; }
private:
	static TInt TickCb(TAny* aSelf);
	void Tick();
	COeContainer* iContainer;
	CPeriodic* iTicker;
};

#endif
