#ifndef IINPUT_H
#define IINPUT_H

#include "interface.h"
#include "VGUI.h"
#include "KeyCode.h"
#include "MouseCode.h"

namespace vgui2
{

class IInput : public IBaseInterface
{
public:
    virtual void SetMouseFocus(VPANEL newMouseFocus) = 0;
    virtual void SetMouseCapture(VPANEL panel) = 0;
    virtual void GetKeyCodeText(KeyCode code, char *buf, int buflen) = 0;
    virtual VPANEL GetFocus() = 0;
    virtual VPANEL GetMouseOver() = 0;
    virtual void SetCursorPos(int x, int y) = 0;
    virtual void GetCursorPos(int &x, int &y) = 0;
    virtual bool WasMousePressed(MouseCode code) = 0;
    virtual bool WasMouseDoublePressed(MouseCode code) = 0;
    virtual bool IsMouseDown(MouseCode code) = 0;
    virtual void SetCursorOveride(HCursor cursor) = 0;
    virtual HCursor GetCursorOveride() = 0;
    virtual bool WasMouseReleased(MouseCode code) = 0;
    virtual bool WasKeyPressed(KeyCode code) = 0;
    virtual bool IsKeyDown(KeyCode code) = 0;
    virtual bool WasKeyTyped(KeyCode code) = 0;
    virtual bool WasKeyReleased(KeyCode code) = 0;
    virtual VPANEL GetAppModalSurface() = 0;
    virtual void SetAppModalSurface(VPANEL panel) = 0;
    virtual void ReleaseAppModalSurface() = 0;
    virtual void GetCursorPosition(int &x, int &y) = 0;
};

#define VGUI_INPUT_INTERFACE_VERSION "VGUI_Input004"

}

#endif // IINPUT_H
