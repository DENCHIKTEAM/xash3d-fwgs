#ifndef ICLIENTVGUI_H
#define ICLIENTVGUI_H

#include "interface.h"
#include "VGUI.h"

class IClientVGUI : public IBaseInterface
{
public:
    virtual void Initialize(CreateInterfaceFn *factories, int count) = 0;
    virtual void Start() = 0;
    virtual void SetParent(vgui2::VPANEL parent) = 0;
    virtual bool UseVGUI1() = 0;
    virtual void HideScoreBoard() = 0;
    virtual void HideAllVGUIMenu() = 0;
    virtual void ActivateClientUI() = 0;
    virtual void HideClientUI() = 0;
    virtual void Shutdown() = 0;
};

#define VCLIENTVGUI_INTERFACE_VERSION "VClientVGUI001"

#endif // ICLIENTVGUI_H
