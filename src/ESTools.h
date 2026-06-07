//============================================================================
//  ESTools.h  -  "EuroScope Tools"  (EuroScope plug-in)
//
//  A small suite of controller tools that share one on-screen panel:
//
//    CRR  - Continuous Range Readout
//           Live distance number next to selected aircraft, to a chosen fix.
//           Colour is a property of the fix; fixes are drawn as "* NAME"
//           (click to abbreviate to first letter). On-scope font size is
//           adjustable.
//
//    PTL  - Predicted Track Line
//           A bright green line projecting where each aircraft will be in
//           N minutes (0-5, 0.5 steps) at its current ground speed/track.
//           Toggle on/off, and scope it to your tracks only or all aircraft.
//
//  Text entry uses ".crr ..." / ".ptl ..." commands; everything else is
//  clickable in the panel.
//
//  License: MIT
//============================================================================
#pragma once

#include <windows.h>
#include <string>
#include <map>
#include <vector>

#include "EuroScopePlugIn.h"

using namespace EuroScopePlugIn;

#define EST_PLUGIN_NAME      "EuroScope Tools"
#define EST_PLUGIN_VERSION   "1.3.0"
#define EST_PLUGIN_AUTHOR    "you"
#define EST_PLUGIN_COPYRIGHT "MIT License"

#define TAG_ITEM_CRR_RANGE   1001
#define EST_OBJ              9100

#define CRR_FONT_MIN         8
#define CRR_FONT_MAX         28

#define PTL_MIN              0.0
#define PTL_MAX              5.0
#define PTL_STEP             0.5
#define PTL_COLOR            RGB(0, 255, 0)   // bright green

struct ESTColor { const char* name; COLORREF rgb; };

struct CRRFix
{
    CPosition   pos;
    COLORREF    rgb;
    bool        showSource;  // scope label shows the source fix instead of the name
    bool        visible;     // drawn on the scope at all
    std::string source;      // sector point the position came from
};

class CESToolsScreen; // fwd

//============================================================================
//  CESToolsPlugin  -  owns all shared state for every tool.
//============================================================================
class CESToolsPlugin : public CPlugIn
{
public:
    CESToolsPlugin(void);
    virtual ~CESToolsPlugin(void);

    CRadarScreen* OnRadarScreenCreated(const char* sDisplayName,
                                       bool NeedRadarContent, bool GeoReferenced,
                                       bool CanBeSaved, bool CanBeCreated) override;

    void OnGetTagItem(CFlightPlan FlightPlan, CRadarTarget RadarTarget,
                      int ItemCode, int TagData, char sItemString[16],
                      int* pColorCode, COLORREF* pRGB, double* pFontSize) override;

    bool OnCompileCommand(const char* sCommandLine) override;

    // --- panel / shared ----------------------------------------------------
    const std::vector<ESTColor>& Palette(void) const { return m_palette; }
    bool     PanelVisible(void) const { return m_panelVisible; }
    void     TogglePanel(void) { m_panelVisible = !m_panelVisible; }
    bool     PanelCollapsed(void) const { return m_panelCollapsed; }
    void     ToggleCollapsed(void) { m_panelCollapsed = !m_panelCollapsed; }
    COLORREF CurrentColor(void) const { return m_currentColor; }
    void     SetCurrentColor(COLORREF c) { m_currentColor = c; }

    // --- CRR ---------------------------------------------------------------
    const std::map<std::string, CRRFix>&      Fixes(void) const { return m_fixes; }
    const std::map<std::string, std::string>& Assignments(void) const { return m_assign; }
    int      FontSize(void) const { return m_fontSize; }
    void     BumpFontSize(int delta);
    void     RemoveFix(const std::string& name);
    void     RemoveAircraft(const std::string& cs);
    void     RecolorFix(const std::string& name, COLORREF rgb);
    void     ToggleFixSource(const std::string& name);   // name <-> source label
    void     ToggleFixVisible(const std::string& name);  // show/hide on the scope

    // --- PTL ---------------------------------------------------------------
    bool     PtlOn(void) const { return m_ptlOn; }
    void     TogglePtl(void) { m_ptlOn = !m_ptlOn; }
    bool     PtlAllAircraft(void) const { return m_ptlAll; }
    void     TogglePtlScope(void) { m_ptlAll = !m_ptlAll; }
    double   PtlMinutes(void) const { return m_ptlMin; }
    void     BumpPtlMinutes(double delta);

    void     ShowCommands(void);   // printed by the ? button

private:
    void CrrHelp(void);
    void PtlHelp(void);
    void CmdList(void);
    bool DefineFix(const std::string& source, const std::string& name, COLORREF rgb);
    void AssignAircraft(const std::string& cs, const std::string& fix);
    bool HandleCrr(const std::vector<std::string>& tk);
    bool HandlePtl(const std::vector<std::string>& tk);
    bool HandleC(const std::vector<std::string>& tk);

    bool        ResolveColor(const std::string& name, COLORREF& out) const;
    std::string ColorName(COLORREF rgb) const;
    void        Msg(const std::string& text, bool warn = false);

    // shared
    std::vector<ESTColor>              m_palette;
    COLORREF                           m_currentColor;
    bool                               m_panelVisible;
    bool                               m_panelCollapsed;

    // CRR
    std::map<std::string, CRRFix>      m_fixes;
    std::map<std::string, std::string> m_assign;
    std::string                        m_lastFix;
    int                                m_fontSize;

    // PTL
    bool                               m_ptlOn;
    bool                               m_ptlAll;   // false = my tracks only
    double                             m_ptlMin;
};

//============================================================================
//  CESToolsScreen  -  drawing + interaction. One per ASR.
//============================================================================
class CESToolsScreen : public CRadarScreen
{
public:
    explicit CESToolsScreen(CESToolsPlugin* plugin);
    virtual ~CESToolsScreen(void);

    void OnRefresh(HDC hDC, int Phase) override;
    void OnClickScreenObject(int ObjectType, const char* sObjectId,
                             POINT Pt, RECT Area, int Button) override;
    void OnMoveScreenObject(int ObjectType, const char* sObjectId,
                            POINT Pt, RECT Area, bool Released) override;
    void OnAsrContentToBeClosed(void) override;

private:
    void   EnsureFont(void);
    bool   Visible(HDC hDC, const POINT& p) const;
    double PxPerNm(const CPosition& at);
    void   DrawPTL(HDC hDC);
    void   DrawFixes(HDC hDC);
    void   DrawReadouts(HDC hDC);
    void   DrawPanel(HDC hDC);

    CESToolsPlugin* m_plugin;
    HFONT           m_font;
    int             m_fontSize;
    HFONT           m_panelFont;
    HFONT           m_panelBold;
    int             m_panelX;
    int             m_panelY;
};
