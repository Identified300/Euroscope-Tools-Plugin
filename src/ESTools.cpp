//============================================================================
//  ESTools.cpp  -  "EuroScope Tools" implementation (CRR + PTL)
//============================================================================
#define _CRT_SECURE_NO_WARNINGS

#include "ESTools.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <sstream>
#include <algorithm>

// ---------------------------------------------------------------------------
static std::string ToUpper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });
    return s;
}
static std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}
static std::vector<std::string> Tokenize(const std::string& line)
{
    std::vector<std::string> out;
    std::istringstream iss(line);
    std::string t;
    while (iss >> t) out.push_back(t);
    return out;
}
static std::string Fmt1(double v)
{
    char b[16]; _snprintf_s(b, sizeof(b), _TRUNCATE, "%.1f", v); return b;
}

// ===========================================================================
//  CESToolsPlugin
// ===========================================================================
CESToolsPlugin::CESToolsPlugin(void)
    : CPlugIn(COMPATIBILITY_CODE, EST_PLUGIN_NAME, EST_PLUGIN_VERSION,
              EST_PLUGIN_AUTHOR, EST_PLUGIN_COPYRIGHT),
      m_currentColor(RGB(0, 220, 255)),
      m_panelVisible(true),
      m_panelCollapsed(false),
      m_fontSize(15),
      m_ptlOn(false),
      m_ptlAll(false),
      m_ptlMin(1.0)
{
    m_palette = {
        { "white",   RGB(235, 235, 235) },
        { "red",     RGB(235,  60,  60) },
        { "orange",  RGB(245, 150,  40) },
        { "yellow",  RGB(235, 220,  60) },
        { "green",   RGB( 70, 215,  90) },
        { "cyan",    RGB(  0, 220, 255) },
        { "blue",    RGB( 70, 140, 245) },
        { "magenta", RGB(225,  90, 220) },
        { "grey",    RGB(160, 160, 160) },
    };

    RegisterTagItemType("CRR range to fix", TAG_ITEM_CRR_RANGE);
    Msg("EuroScope Tools loaded. Type .crr help or .ptl help.");
}

CESToolsPlugin::~CESToolsPlugin(void) {}

CRadarScreen* CESToolsPlugin::OnRadarScreenCreated(const char*, bool, bool, bool, bool)
{
    return new CESToolsScreen(this);
}

void CESToolsPlugin::BumpFontSize(int delta)
{
    m_fontSize += delta;
    if (m_fontSize < CRR_FONT_MIN) m_fontSize = CRR_FONT_MIN;
    if (m_fontSize > CRR_FONT_MAX) m_fontSize = CRR_FONT_MAX;
}

void CESToolsPlugin::BumpPtlMinutes(double delta)
{
    m_ptlMin += delta;
    // snap to the nearest 0.5 and clamp
    m_ptlMin = std::round(m_ptlMin / PTL_STEP) * PTL_STEP;
    if (m_ptlMin < PTL_MIN) m_ptlMin = PTL_MIN;
    if (m_ptlMin > PTL_MAX) m_ptlMin = PTL_MAX;
}

// ---------------------------------------------------------------------------
void CESToolsPlugin::OnGetTagItem(CFlightPlan, CRadarTarget RadarTarget, int ItemCode,
                                  int, char sItemString[16], int* pColorCode,
                                  COLORREF* pRGB, double*)
{
    if (ItemCode != TAG_ITEM_CRR_RANGE) return;
    if (!RadarTarget.IsValid())         return;
    auto a = m_assign.find(RadarTarget.GetCallsign());
    if (a == m_assign.end())            return;
    auto f = m_fixes.find(a->second);
    if (f == m_fixes.end())             return;
    CRadarTargetPositionData pos = RadarTarget.GetPosition();
    if (!pos.IsValid())                 return;

    double dist = pos.GetPosition().DistanceTo(f->second.pos);
    _snprintf_s(sItemString, 16, _TRUNCATE, "%.0f", dist);
    *pColorCode = TAG_COLOR_RGB_DEFINED;
    *pRGB       = f->second.rgb;
}

// ---------------------------------------------------------------------------
//  Command routing: ".crr ..." and ".ptl ..."
// ---------------------------------------------------------------------------
bool CESToolsPlugin::OnCompileCommand(const char* sCommandLine)
{
    std::vector<std::string> tk = Tokenize(sCommandLine ? sCommandLine : "");
    if (tk.empty()) return false;
    std::string head = ToLower(tk[0]);
    if (head == ".crr") return HandleCrr(tk);
    if (head == ".ptl") return HandlePtl(tk);
    if (head == ".c" || head == ".cr") return HandleC(tk);
    return false;
}

// ---------------------------------------------------------------------------
//  ".c" - the quick command.
//    .c <FIX> <NAME>        define a fix at sector point FIX, called NAME (green)
//    .c <FIX>               define a fix, name = the sector point
//    .c <CALLSIGN> <NAME>   if NAME is an existing fix, show CALLSIGN's range to it
// ---------------------------------------------------------------------------
bool CESToolsPlugin::HandleC(const std::vector<std::string>& tk)
{
    if (tk.size() < 2) { ShowCommands(); return true; }

    std::string a = ToUpper(tk[1]);

    if (tk.size() >= 3)
    {
        std::string b = ToUpper(tk[2]);
        if (m_fixes.count(b))           // B already names a fix -> add aircraft A to it
        {
            AssignAircraft(a, b);
            return true;
        }
        COLORREF rgb = RGB(70, 215, 90); // default green
        if (tk.size() >= 4) ResolveColor(tk[3], rgb);
        DefineFix(a, b, rgb);           // define fix from point A, called B
        return true;
    }

    DefineFix(a, a, RGB(70, 215, 90));  // single token: name = the point, green
    return true;
}

bool CESToolsPlugin::HandleCrr(const std::vector<std::string>& tk)
{
    if (tk.size() == 1) { TogglePanel(); return true; }
    std::string sub = ToLower(tk[1]);

    if (sub == "help" || sub == "?") { CrrHelp(); return true; }
    if (sub == "panel" || sub == "ui") { TogglePanel(); return true; }
    if (sub == "collapse") { ToggleCollapsed(); return true; }
    if (sub == "list") { CmdList(); return true; }
    if (sub == "clear")
    {
        m_fixes.clear(); m_assign.clear(); m_lastFix.clear();
        Msg("CRR: cleared all fixes and aircraft."); return true;
    }
    if (sub == "font")
    {
        if (tk.size() < 3) { Msg("usage: .crr font <size>", true); return true; }
        m_fontSize = 0; BumpFontSize(atoi(tk[2].c_str()));
        Msg("CRR: on-scope font size = " + std::to_string(m_fontSize)); return true;
    }
    if (sub == "abbrev" || sub == "abbreviate" || sub == "label")
    {
        if (tk.size() < 3) { Msg("usage: .crr label <FIX>", true); return true; }
        ToggleFixSource(ToUpper(tk[2])); return true;
    }
    if (sub == "fix")
    {
        if (tk.size() < 3) { Msg("usage: .crr fix <NAME> [colour]", true); return true; }
        std::string name = ToUpper(tk[2]);
        COLORREF rgb = m_currentColor;
        if (tk.size() >= 4 && !ResolveColor(tk[3], rgb))
        { Msg("unknown colour '" + tk[3] + "'; using current.", true); rgb = m_currentColor; }
        DefineFix(name, name, rgb); return true;
    }
    if (sub == "add")
    {
        if (tk.size() < 3) { Msg("usage: .crr add <CALLSIGN> [FIX]", true); return true; }
        std::string cs  = ToUpper(tk[2]);
        std::string fix = (tk.size() >= 4) ? ToUpper(tk[3]) : m_lastFix;
        if (fix.empty())         { Msg("define a fix first: .crr fix <NAME> <colour>", true); return true; }
        if (!m_fixes.count(fix)) { Msg("fix '" + fix + "' is not defined.", true); return true; }
        AssignAircraft(cs, fix); return true;
    }
    if (sub == "remove" || sub == "rm" || sub == "del")
    {
        if (tk.size() < 3) { Msg("usage: .crr remove <CALLSIGN>", true); return true; }
        std::string cs = ToUpper(tk[2]); RemoveAircraft(cs);
        Msg("CRR: removed " + cs + "."); return true;
    }
    if (sub == "delfix")
    {
        if (tk.size() < 3) { Msg("usage: .crr delfix <NAME>", true); return true; }
        std::string name = ToUpper(tk[2]); RemoveFix(name);
        Msg("CRR: deleted fix " + name + "."); return true;
    }
    if (sub == "color" || sub == "colour")
    {
        if (tk.size() >= 4)
        {
            std::string name = ToUpper(tk[2]); COLORREF rgb;
            if (!ResolveColor(tk[3], rgb)) { Msg("unknown colour '" + tk[3] + "'.", true); return true; }
            if (m_fixes.count(name)) { RecolorFix(name, rgb); Msg("CRR: recoloured " + name + "."); }
            else                     { Msg("fix '" + name + "' is not defined.", true); }
            return true;
        }
        Msg("usage: .crr color <FIX> <colour>", true); return true;
    }
    Msg("unknown CRR command. Type .crr help.", true); return true;
}

bool CESToolsPlugin::HandlePtl(const std::vector<std::string>& tk)
{
    if (tk.size() == 1) { TogglePtl(); Msg(std::string("PTL ") + (m_ptlOn ? "on." : "off.")); return true; }
    std::string sub = ToLower(tk[1]);

    if (sub == "help" || sub == "?") { PtlHelp(); return true; }
    if (sub == "on")     { m_ptlOn = true;  Msg("PTL on.");  return true; }
    if (sub == "off")    { m_ptlOn = false; Msg("PTL off."); return true; }
    if (sub == "toggle") { TogglePtl(); Msg(std::string("PTL ") + (m_ptlOn ? "on." : "off.")); return true; }
    if (sub == "mine" || sub == "me")  { m_ptlAll = false; Msg("PTL: my tracks only."); return true; }
    if (sub == "all")                  { m_ptlAll = true;  Msg("PTL: all aircraft.");   return true; }
    if (sub == "len" || sub == "length" || sub == "min")
    {
        if (tk.size() < 3) { Msg("usage: .ptl len <0-5>", true); return true; }
        m_ptlMin = 0.0; BumpPtlMinutes(atof(tk[2].c_str()));
        Msg("PTL length = " + Fmt1(m_ptlMin) + " min"); return true;
    }
    Msg("unknown PTL command. Type .ptl help.", true); return true;
}

// ---------------------------------------------------------------------------
void CESToolsPlugin::CrrHelp(void)
{
    Msg("CRR commands:");
    Msg("  .crr fix <NAME> [colour]   define/colour a fix (drawn as * NAME)");
    Msg("  .crr add <CS> [FIX]        show CS's range to FIX (last fix if omitted)");
    Msg("  .crr remove <CS>           stop showing an aircraft");
    Msg("  .crr color <FIX> <colour>  recolour a fix");
    Msg("  .crr abbrev <FIX>          full name / first letter on the scope");
    Msg("  .crr delfix <NAME>         delete a fix and its aircraft");
    Msg("  .crr font <size>           on-scope text size (8-28)");
    Msg("  .crr list / clear / panel / collapse");
}

void CESToolsPlugin::PtlHelp(void)
{
    Msg("PTL commands (Predicted Track Line):");
    Msg("  .ptl on / off / toggle     bright green vector line");
    Msg("  .ptl mine                  only aircraft I track");
    Msg("  .ptl all                   all aircraft");
    Msg("  .ptl len <0-5>             length in minutes (0.5 steps)");
}

void CESToolsPlugin::ShowCommands(void)
{
    Msg("EuroScope Tools - commands (.c or .cr):");
    Msg("  .cr <FIX> <NAME>      add a fix at point FIX, called NAME (green)");
    Msg("  .cr <CALLSIGN> <NAME> show that aircraft's range to fix NAME");
    Msg("  .ptl on / off         predicted track line");
    Msg("  .ptl mine / all       PTL: my tracks only / all aircraft");
    Msg("  .ptl len <0-5>        PTL length in minutes");
    Msg("  panel: swatch sets colour, click a fix to recolour, click its");
    Msg("         scope label to abbreviate, x to remove.");
    Msg("  full help: .crr help  /  .ptl help");
}

void CESToolsPlugin::CmdList(void)
{
    if (m_fixes.empty()) { Msg("CRR: no fixes defined."); return; }
    for (const auto& f : m_fixes)
        Msg("fix " + f.first + " (" + f.second.source + ") [" + ColorName(f.second.rgb) + "]");
    if (m_assign.empty()) { Msg("CRR: no aircraft assigned."); return; }
    for (const auto& a : m_assign)
        Msg("  " + a.first + " -> " + a.second);
}

// ---------------------------------------------------------------------------
bool CESToolsPlugin::DefineFix(const std::string& source, const std::string& name, COLORREF rgb)
{
    const int kinds[] = { SECTOR_ELEMENT_FIX, SECTOR_ELEMENT_VOR,
                          SECTOR_ELEMENT_NDB, SECTOR_ELEMENT_AIRPORT };
    for (int kind : kinds)
    {
        for (CSectorElement se = SectorFileElementSelectFirst(kind);
             se.IsValid(); se = SectorFileElementSelectNext(se, kind))
        {
            const char* n = se.GetName();
            if (n && ToUpper(n) == source)
            {
                CPosition p;
                if (se.GetPosition(&p, 0))
                {
                    bool src = false, vis = true;
                    auto ex = m_fixes.find(name);
                    if (ex != m_fixes.end()) { src = ex->second.showSource; vis = ex->second.visible; }
                    m_fixes[name] = CRRFix{ p, rgb, src, vis, source };
                    m_lastFix = name;
                    if (source == name) Msg("CRR: fix " + name + " set.");
                    else                Msg("CRR: fix " + name + " (" + source + ") set.");
                    return true;
                }
            }
        }
    }
    Msg("point '" + source + "' not found in the active sector file.", true);
    return false;
}

void CESToolsPlugin::AssignAircraft(const std::string& cs, const std::string& fix)
{ m_assign[cs] = fix; Msg("CRR: " + cs + " -> " + fix + "."); }

void CESToolsPlugin::RemoveAircraft(const std::string& cs) { m_assign.erase(cs); }

void CESToolsPlugin::RemoveFix(const std::string& name)
{
    m_fixes.erase(name);
    for (auto it = m_assign.begin(); it != m_assign.end(); )
        if (it->second == name) it = m_assign.erase(it); else ++it;
    if (m_lastFix == name) m_lastFix.clear();
}

void CESToolsPlugin::RecolorFix(const std::string& name, COLORREF rgb)
{ auto it = m_fixes.find(name); if (it != m_fixes.end()) it->second.rgb = rgb; }

void CESToolsPlugin::ToggleFixSource(const std::string& name)
{ auto it = m_fixes.find(name); if (it != m_fixes.end()) it->second.showSource = !it->second.showSource; }

void CESToolsPlugin::ToggleFixVisible(const std::string& name)
{ auto it = m_fixes.find(name); if (it != m_fixes.end()) it->second.visible = !it->second.visible; }

bool CESToolsPlugin::ResolveColor(const std::string& name, COLORREF& out) const
{
    std::string n = ToLower(name);
    for (const auto& c : m_palette) if (n == c.name) { out = c.rgb; return true; }
    int r, g, b;
    if (sscanf(name.c_str(), "%d,%d,%d", &r, &g, &b) == 3)
    { out = RGB(r & 0xFF, g & 0xFF, b & 0xFF); return true; }
    return false;
}

std::string CESToolsPlugin::ColorName(COLORREF rgb) const
{
    for (const auto& c : m_palette) if (c.rgb == rgb) return c.name;
    return "custom";
}

void CESToolsPlugin::Msg(const std::string& text, bool warn)
{ DisplayUserMessage(EST_PLUGIN_NAME, "EST", text.c_str(), true, true, false, warn, false); }

// ===========================================================================
//  CESToolsScreen
// ===========================================================================
CESToolsScreen::CESToolsScreen(CESToolsPlugin* plugin)
    : m_plugin(plugin), m_font(nullptr), m_fontSize(0), m_panelX(60), m_panelY(90)
{
    m_panelFont = CreateFontA(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    m_panelBold = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                         ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
}

CESToolsScreen::~CESToolsScreen(void)
{
    if (m_font)      DeleteObject(m_font);
    if (m_panelFont) DeleteObject(m_panelFont);
    if (m_panelBold) DeleteObject(m_panelBold);
}

void CESToolsScreen::OnAsrContentToBeClosed(void) { delete this; }

void CESToolsScreen::EnsureFont(void)
{
    int want = m_plugin->FontSize();
    if (m_font && want == m_fontSize) return;
    if (m_font) DeleteObject(m_font);
    m_font = CreateFontA(want, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                         ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");
    m_fontSize = want;
}

bool CESToolsScreen::Visible(HDC hDC, const POINT& p) const
{
    RECT c; GetClipBox(hDC, &c);
    const int m = 200;
    return p.x >= c.left - m && p.x <= c.right + m &&
           p.y >= c.top  - m && p.y <= c.bottom + m;
}

// pixels per nautical mile at a position (1' latitude ~= 1 nm)
double CESToolsScreen::PxPerNm(const CPosition& at)
{
    CPosition a = at, b = at;
    b.m_Latitude += 10.0 / 60.0;     // 10 nm north
    POINT pa = ConvertCoordFromPositionToPixel(a);
    POINT pb = ConvertCoordFromPositionToPixel(b);
    double dx = pb.x - pa.x, dy = pb.y - pa.y;
    return std::sqrt(dx * dx + dy * dy) / 10.0;
}

void CESToolsScreen::OnRefresh(HDC hDC, int Phase)
{
    if (Phase != REFRESH_PHASE_AFTER_TAGS) return;
    EnsureFont();
    DrawPTL(hDC);
    DrawFixes(hDC);
    DrawReadouts(hDC);
    if (m_plugin->PanelVisible()) DrawPanel(hDC);
}

// ---------------------------------------------------------------------------
//  PTL - bright green predicted track line.
// ---------------------------------------------------------------------------
void CESToolsScreen::DrawPTL(HDC hDC)
{
    if (!m_plugin->PtlOn()) return;
    double mins = m_plugin->PtlMinutes();
    if (mins <= 0.0) return;
    bool mineOnly = !m_plugin->PtlAllAircraft();

    HPEN pen = CreatePen(PS_SOLID, 1, PTL_COLOR);
    HPEN old = (HPEN)SelectObject(hDC, pen);

    for (CRadarTarget rt = m_plugin->RadarTargetSelectFirst();
         rt.IsValid(); rt = m_plugin->RadarTargetSelectNext(rt))
    {
        CRadarTargetPositionData cur = rt.GetPosition();
        if (!cur.IsValid()) continue;

        int gs = rt.GetGS();
        if (gs < 50) continue;                       // skip taxiing / slow traffic

        if (mineOnly)
        {
            CFlightPlan fp = rt.GetCorrelatedFlightPlan();
            if (!fp.IsValid() || !fp.GetTrackingControllerIsMe()) continue;
        }

        CPosition curPos = cur.GetPosition();
        POINT p0 = ConvertCoordFromPositionToPixel(curPos);
        if (!Visible(hDC, p0)) continue;

        CRadarTargetPositionData prev = rt.GetPreviousPosition(cur);
        if (!prev.IsValid()) continue;
        POINT pp = ConvertCoordFromPositionToPixel(prev.GetPosition());

        double dx = (double)(p0.x - pp.x), dy = (double)(p0.y - pp.y);
        double len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.5) continue;                     // no usable direction
        dx /= len; dy /= len;

        double pxPerNm = PxPerNm(curPos);
        if (pxPerNm <= 0.0) continue;

        double distNm = gs * (mins / 60.0);
        double L = distNm * pxPerNm;

        MoveToEx(hDC, p0.x, p0.y, NULL);
        LineTo(hDC, p0.x + (int)(dx * L), p0.y + (int)(dy * L));
    }

    SelectObject(hDC, old);
    DeleteObject(pen);
}

// ---------------------------------------------------------------------------
//  "* NAME" at each fix; click to toggle full name / first letter.
// ---------------------------------------------------------------------------
void CESToolsScreen::DrawFixes(HDC hDC)
{
    HFONT oldFont = (HFONT)SelectObject(hDC, m_font);
    int   oldBk   = SetBkMode(hDC, TRANSPARENT);

    for (const auto& f : m_plugin->Fixes())
    {
        if (!f.second.visible) continue;             // toggled off the scope

        POINT p = ConvertCoordFromPositionToPixel(f.second.pos);
        if (!Visible(hDC, p)) continue;

        std::string disp = f.second.showSource ? f.second.source : f.first;
        std::string label = "* " + disp;
        SetTextColor(hDC, f.second.rgb);
        TextOutA(hDC, p.x - 4, p.y - 7, label.c_str(), (int)label.length());

        SIZE sz; GetTextExtentPoint32A(hDC, label.c_str(), (int)label.length(), &sz);
        RECT r = { p.x - 4, p.y - 7, p.x - 4 + sz.cx, p.y - 7 + sz.cy };
        std::string id = "FIXLBL|" + f.first;
        AddScreenObject(EST_OBJ, id.c_str(), r, false, "Click: toggle name / fix");
    }

    SetBkMode(hDC, oldBk);
    SelectObject(hDC, oldFont);
}

// ---------------------------------------------------------------------------
//  CRR readout: rounded distance number in fix colour next to the aircraft.
// ---------------------------------------------------------------------------
void CESToolsScreen::DrawReadouts(HDC hDC)
{
    const auto& fixes = m_plugin->Fixes();
    HFONT oldFont = (HFONT)SelectObject(hDC, m_font);
    int   oldBk   = SetBkMode(hDC, TRANSPARENT);

    for (const auto& a : m_plugin->Assignments())
    {
        auto f = fixes.find(a.second);
        if (f == fixes.end()) continue;
        if (!f->second.visible) continue;            // fix toggled off -> hide its numbers
        CRadarTarget rt = m_plugin->RadarTargetSelect(a.first.c_str());
        if (!rt.IsValid()) continue;
        CRadarTargetPositionData pd = rt.GetPosition();
        if (!pd.IsValid()) continue;

        CPosition acPos = pd.GetPosition();
        POINT pt = ConvertCoordFromPositionToPixel(acPos);
        if (!Visible(hDC, pt)) continue;

        double dist = acPos.DistanceTo(f->second.pos);
        char buf[16]; _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%.0f", dist);

        int tx = pt.x + 12, ty = pt.y + 10;
        SetTextColor(hDC, f->second.rgb);
        TextOutA(hDC, tx, ty, buf, (int)strlen(buf));

        SIZE sz; GetTextExtentPoint32A(hDC, buf, (int)strlen(buf), &sz);
        RECT r = { tx, ty, tx + sz.cx, ty + sz.cy };
        std::string id = "RDO|" + a.first;
        AddScreenObject(EST_OBJ, id.c_str(), r, false, "Left-click: stop showing");
    }

    SetBkMode(hDC, oldBk);
    SelectObject(hDC, oldFont);
}

// ---------------------------------------------------------------------------
static void Box(HDC hDC, RECT r, COLORREF stroke, int width)
{
    HPEN p  = CreatePen(PS_SOLID, width, stroke);
    HPEN op = (HPEN)SelectObject(hDC, p);
    HBRUSH ob = (HBRUSH)SelectObject(hDC, GetStockObject(NULL_BRUSH));
    Rectangle(hDC, r.left, r.top, r.right, r.bottom);
    SelectObject(hDC, ob); SelectObject(hDC, op); DeleteObject(p);
}

static COLORREF Dim(COLORREF c)
{
    return RGB(GetRValue(c) / 3, GetGValue(c) / 3, GetBValue(c) / 3);
}

void CESToolsScreen::DrawPanel(HDC hDC)
{
    const auto& pal    = m_plugin->Palette();
    const auto& fixes  = m_plugin->Fixes();
    const auto& assign = m_plugin->Assignments();
    const bool  collapsed = m_plugin->PanelCollapsed();

    const int W = 224, pad = 8, rowH = 18, hdrH = 22, swSize = 16, swGap = 4;
    const int X = m_panelX, Y = m_panelY;

    int H = hdrH;
    if (!collapsed)
    {
        int crrRows = 1;                       // "CRR fixes" label
        if (fixes.empty()) crrRows += 1;       // hint line
        else for (const auto& f : fixes)
        {
            crrRows += 1;                      // bold fix line
            for (const auto& a : assign)
                if (a.second == f.first) crrRows += 1;   // its aircraft
        }

        H += (swSize + 8)
           + rowH                              // font
           + crrRows * rowH                    // CRR section
           + 8 + rowH + rowH + rowH            // PTL block
           + pad + 2;
    }

    RECT panel = { X, Y, X + W, Y + H };
    HBRUSH bg = CreateSolidBrush(RGB(24, 26, 32));
    FillRect(hDC, &panel, bg); DeleteObject(bg);
    Box(hDC, panel, RGB(80, 86, 100), 1);

    HFONT oldFont = (HFONT)SelectObject(hDC, m_panelFont);
    SetBkMode(hDC, TRANSPARENT);

    RECT hdr = { X, Y, X + W, Y + hdrH };
    HBRUSH hb = CreateSolidBrush(RGB(40, 44, 54));
    FillRect(hDC, &hdr, hb); DeleteObject(hb);
    SetTextColor(hDC, RGB(235, 235, 235));
    TextOutA(hDC, X + pad, Y + 4, "ES Tools", 8);

    RECT drag = { X, Y, X + W - 42, Y + hdrH };
    AddScreenObject(EST_OBJ, "HDR", drag, true, "Drag to move");

    RECT helpb = { X + W - 40, Y + 3, X + W - 24, Y + 19 };
    Box(hDC, helpb, RGB(120, 126, 140), 1);
    SetTextColor(hDC, RGB(220, 220, 220));
    TextOutA(hDC, X + W - 36, Y + 3, "?", 1);
    AddScreenObject(EST_OBJ, "HELP", helpb, false, "Show commands");

    RECT col = { X + W - 20, Y + 3, X + W - 4, Y + 19 };
    Box(hDC, col, RGB(120, 126, 140), 1);
    SetTextColor(hDC, RGB(220, 220, 220));
    TextOutA(hDC, X + W - 16, Y + 3, collapsed ? "+" : "-", 1);
    AddScreenObject(EST_OBJ, "COL", col, false, collapsed ? "Expand" : "Collapse");

    if (collapsed) { SelectObject(hDC, oldFont); return; }

    int cy = Y + hdrH + 6;

    // ---- swatches ----
    {
        int sx = X + pad;
        for (size_t i = 0; i < pal.size(); ++i)
        {
            RECT sw = { sx, cy, sx + swSize, cy + swSize };
            HBRUSH b = CreateSolidBrush(pal[i].rgb); FillRect(hDC, &sw, b); DeleteObject(b);
            bool sel = (pal[i].rgb == m_plugin->CurrentColor());
            Box(hDC, sw, sel ? RGB(255,255,255) : RGB(70,74,84), sel ? 2 : 1);
            char id[16]; _snprintf_s(id, sizeof(id), _TRUNCATE, "SW%d", (int)i);
            AddScreenObject(EST_OBJ, id, sw, false, pal[i].name);
            sx += swSize + swGap;
        }
        cy += swSize + 8;
    }

    // ---- font stepper ----
    {
        SetTextColor(hDC, RGB(150, 154, 164));
        TextOutA(hDC, X + pad, cy, "Font", 4);
        RECT fb = { X + pad + 38, cy - 2, X + pad + 72, cy + 15 };
        Box(hDC, fb, RGB(90, 96, 110), 1);
        char fs[8]; _snprintf_s(fs, sizeof(fs), _TRUNCATE, "%d", m_plugin->FontSize());
        SetTextColor(hDC, RGB(220, 220, 220));
        TextOutA(hDC, fb.left + 9, cy, fs, (int)strlen(fs));
        AddScreenObject(EST_OBJ, "FONT", fb, false, "Left +2  Right -2");
        cy += rowH;
    }

    // ---- CRR fixes (bold) with their aircraft (callsign only) underneath ----
    SetTextColor(hDC, RGB(150, 154, 164));
    TextOutA(hDC, X + pad, cy, "CRR fixes", 9);
    cy += rowH;
    if (fixes.empty())
    {
        SetTextColor(hDC, RGB(130, 130, 130));
        TextOutA(hDC, X + pad + 4, cy, ".c FIX NAME", 11); cy += rowH;
    }
    else for (const auto& f : fixes)
    {
        bool     vis = f.second.visible;
        COLORREF fc  = vis ? f.second.rgb : Dim(f.second.rgb);

        // colour chip: click to apply the currently selected swatch colour
        RECT chip = { X + pad, cy + 2, X + pad + 12, cy + 14 };
        HBRUSH cb = CreateSolidBrush(f.second.rgb);
        FillRect(hDC, &chip, cb); DeleteObject(cb);
        Box(hDC, chip, RGB(70, 74, 84), 1);
        AddScreenObject(EST_OBJ, ("FIXCOL|" + f.first).c_str(), chip, false,
                        "Click: set this fix to the selected swatch colour");

        // bold fix name (left-click show/hide, right-click recolour)
        SelectObject(hDC, m_panelBold);
        RECT nameR = { X + pad + 18, cy, X + W - pad - 14, cy + rowH };
        SetTextColor(hDC, fc);
        TextOutA(hDC, nameR.left, cy + 1, f.first.c_str(), (int)f.first.length());
        AddScreenObject(EST_OBJ, ("FIXTOG|" + f.first).c_str(), nameR, false,
                        "Left-click: show/hide on scope");
        SelectObject(hDC, m_panelFont);

        RECT fdel = { X + W - pad - 12, cy, X + W - pad, cy + rowH };
        SetTextColor(hDC, RGB(200, 120, 120));
        TextOutA(hDC, fdel.left, cy + 1, "x", 1);
        AddScreenObject(EST_OBJ, ("FIXDEL|" + f.first).c_str(), fdel, false, "Delete fix");
        cy += rowH;

        // aircraft on this fix - callsign only, indented
        for (const auto& a : assign)
        {
            if (a.second != f.first) continue;
            SetTextColor(hDC, vis ? RGB(205, 208, 214) : RGB(120, 122, 128));
            TextOutA(hDC, X + pad + 18, cy + 1, a.first.c_str(), (int)a.first.length());
            RECT adel = { X + W - pad - 12, cy, X + W - pad, cy + rowH };
            SetTextColor(hDC, RGB(200, 120, 120));
            TextOutA(hDC, adel.left, cy + 1, "x", 1);
            AddScreenObject(EST_OBJ, ("ACDEL|" + a.first).c_str(), adel, false, "Stop showing");
            cy += rowH;
        }
    }

    // ---- divider ----
    cy += 4;
    { HPEN p = CreatePen(PS_SOLID, 1, RGB(60, 64, 74)); HPEN op = (HPEN)SelectObject(hDC, p);
      MoveToEx(hDC, X + pad, cy, NULL); LineTo(hDC, X + W - pad, cy);
      SelectObject(hDC, op); DeleteObject(p); }
    cy += 4;

    // ---- PTL block ----
    SetTextColor(hDC, RGB(150, 154, 164));
    TextOutA(hDC, X + pad, cy, "PTL", 3);
    cy += rowH;
    {
        bool on  = m_plugin->PtlOn();
        bool all = m_plugin->PtlAllAircraft();

        RECT onb = { X + pad, cy - 2, X + pad + 40, cy + 15 };
        Box(hDC, onb, on ? RGB(0,180,0) : RGB(90,96,110), 1);
        SetTextColor(hDC, on ? RGB(0,255,0) : RGB(160,160,160));
        TextOutA(hDC, onb.left + 8, cy, on ? "ON" : "OFF", on ? 2 : 3);
        AddScreenObject(EST_OBJ, "PTLON", onb, false, "Toggle PTL on/off");

        RECT scb = { X + pad + 50, cy - 2, X + pad + 50 + 54, cy + 15 };
        Box(hDC, scb, RGB(90, 96, 110), 1);
        SetTextColor(hDC, RGB(210, 210, 210));
        TextOutA(hDC, scb.left + 8, cy, all ? "All a/c" : "My trks", 7);
        AddScreenObject(EST_OBJ, "PTLSCOPE", scb, false, "Scope: my tracks / all aircraft");
        cy += rowH;
    }
    {
        SetTextColor(hDC, RGB(150, 154, 164));
        TextOutA(hDC, X + pad, cy, "Length", 6);
        RECT lb = { X + pad + 48, cy - 2, X + pad + 48 + 36, cy + 15 };
        Box(hDC, lb, RGB(90, 96, 110), 1);
        std::string lv = Fmt1(m_plugin->PtlMinutes());
        SetTextColor(hDC, RGB(220, 220, 220));
        TextOutA(hDC, lb.left + 6, cy, lv.c_str(), (int)lv.length());
        AddScreenObject(EST_OBJ, "PTLLEN", lb, false, "Left +0.5  Right -0.5  (0-5 min)");
        SetTextColor(hDC, RGB(120, 124, 134));
        TextOutA(hDC, lb.right + 6, cy, "min", 3);
        cy += rowH;
    }

    SelectObject(hDC, oldFont);
}

// ---------------------------------------------------------------------------
void CESToolsScreen::OnClickScreenObject(int ObjectType, const char* sObjectId,
                                         POINT, RECT, int Button)
{
    if (ObjectType != EST_OBJ || !sObjectId) return;
    std::string id = sObjectId;

    if (id == "COL")      { m_plugin->ToggleCollapsed(); return; }
    if (id == "HELP")     { m_plugin->ShowCommands(); return; }
    if (id == "FONT")     { m_plugin->BumpFontSize(Button == BUTTON_RIGHT ? -2 : +2); return; }
    if (id == "PTLON")    { m_plugin->TogglePtl(); return; }
    if (id == "PTLSCOPE") { m_plugin->TogglePtlScope(); return; }
    if (id == "PTLLEN")   { m_plugin->BumpPtlMinutes(Button == BUTTON_RIGHT ? -PTL_STEP : +PTL_STEP); return; }

    if (id.rfind("SW", 0) == 0)
    {
        size_t idx = (size_t)atoi(id.c_str() + 2);
        const auto& pal = m_plugin->Palette();
        if (idx < pal.size()) m_plugin->SetCurrentColor(pal[idx].rgb);
        return;
    }
    if (id.rfind("FIXLBL|", 0) == 0) { m_plugin->ToggleFixSource(id.substr(7)); return; }
    if (id.rfind("FIXCOL|", 0) == 0) { m_plugin->RecolorFix(id.substr(7), m_plugin->CurrentColor()); return; }
    if (id.rfind("FIXTOG|", 0) == 0)
    {
        if (Button == BUTTON_RIGHT) m_plugin->RecolorFix(id.substr(7), m_plugin->CurrentColor());
        else                        m_plugin->ToggleFixVisible(id.substr(7));
        return;
    }
    if (id.rfind("FIXDEL|", 0) == 0) { m_plugin->RemoveFix(id.substr(7)); return; }
    if (id.rfind("ACDEL|",  0) == 0) { m_plugin->RemoveAircraft(id.substr(6)); return; }
    if (id.rfind("RDO|",    0) == 0) { m_plugin->RemoveAircraft(id.substr(4)); return; }
}

// ---------------------------------------------------------------------------
void CESToolsScreen::OnMoveScreenObject(int ObjectType, const char* sObjectId,
                                        POINT Pt, RECT Area, bool)
{
    if (ObjectType != EST_OBJ || !sObjectId) return;
    if (std::string(sObjectId) != "HDR") return;
    m_panelX = Pt.x - (Area.right - Area.left) / 2;
    m_panelY = Pt.y - (Area.bottom - Area.top) / 2;
    if (m_panelX < 0) m_panelX = 0;
    if (m_panelY < 0) m_panelY = 0;
}

// ===========================================================================
//  DLL entry points
// ===========================================================================
static CESToolsPlugin* g_plugin = nullptr;

void __declspec(dllexport) EuroScopePlugInInit(CPlugIn** ppPlugInInstance)
{ *ppPlugInInstance = g_plugin = new CESToolsPlugin(); }

void __declspec(dllexport) EuroScopePlugInExit(void)
{ delete g_plugin; g_plugin = nullptr; }
