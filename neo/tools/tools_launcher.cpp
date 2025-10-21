#include "tools/edit_gui_common.h"
#include "framework/Common.h"

static CToolsLauncherWnd *g_toolsLauncher = NULL;

class CToolsLauncherWnd : public CFrameWnd {
public:
    CToolsLauncherWnd() {
        Create( NULL, "Tools Launcher", WS_OVERLAPPEDWINDOW, rectDefault, NULL, NULL );

        int y = 10;
        int x = 10;

        CButton *radiant = new CButton();
        radiant->Create( "Radiant", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect( x, y, x + 100, y + 30 ), this, 1001 );

        y += 40;
        CButton *gui = new CButton();
        gui->Create( "GUI", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect( x, y, x + 100, y + 30 ), this, 1002 );

        y += 40;
        CButton *script = new CButton();
        script->Create( "Script", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect( x, y, x + 100, y + 30 ), this, 1003 );

        y += 40;
        CButton *light = new CButton();
        light->Create( "Light", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect( x, y, x + 100, y + 30 ), this, 1004 );

        y += 40;
        CButton *sound = new CButton();
        sound->Create( "Sound", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect( x, y, x + 100, y + 30 ), this, 1005 );

        y += 40;
        CButton *af = new CButton();
        af->Create( "AF", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect( x, y, x + 100, y + 30 ), this, 1006 );

        y = 10;
        x = 120;
        CButton *particle = new CButton();
        particle->Create( "Particle", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect( x, y, x + 100, y + 30 ), this, 1007 );

        y += 40;
        CButton *pda = new CButton();
        pda->Create( "PDA", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect( x, y, x + 100, y + 30 ), this, 1008 );

        y += 40;
        CButton *aas = new CButton();
        aas->Create( "AAS", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect( x, y, x + 100, y + 30 ), this, 1009 );

        y += 40;
        CButton *material = new CButton();
        material->Create( "Material", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect( x, y, x + 100, y + 30 ), this, 1010 );

        y += 40;
        CButton *decl = new CButton();
        decl->Create( "DECL", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect( x, y, x + 100, y + 30 ), this, 1011 );
    }

    void OnRadiant() {
        common->InitTool( EDITOR_RADIANT, NULL );
        DestroyWindow();
    }

    void OnGui() {
        common->InitTool( EDITOR_GUI, NULL );
        DestroyWindow();
    }

    void OnScript() {
        common->InitTool( EDITOR_SCRIPT, NULL );
        DestroyWindow();
    }

    void OnLight() {
        common->InitTool( EDITOR_LIGHT, NULL );
        DestroyWindow();
    }

    void OnSound() {
        common->InitTool( EDITOR_SOUND, NULL );
        DestroyWindow();
    }

    void OnAF() {
        common->InitTool( EDITOR_AF, NULL );
        DestroyWindow();
    }

    void OnParticle() {
        common->InitTool( EDITOR_PARTICLE, NULL );
        DestroyWindow();
    }

    void OnPDA() {
        common->InitTool( EDITOR_PDA, NULL );
        DestroyWindow();
    }

    void OnAAS() {
        common->InitTool( EDITOR_AAS, NULL );
        DestroyWindow();
    }

    void OnMaterial() {
        common->InitTool( EDITOR_MATERIAL, NULL );
        DestroyWindow();
    }

    void OnDECL() {
        common->InitTool( EDITOR_DECL, NULL );
        DestroyWindow();
    }

    void PostNcDestroy() {
        CFrameWnd::PostNcDestroy();
        com_editors &= ~EDITOR_TOOLSLAUNCHER;
        delete this;
        g_toolsLauncher = NULL;
    }

    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP( CToolsLauncherWnd, CFrameWnd )
    ON_COMMAND( 1001, OnRadiant )
    ON_COMMAND( 1002, OnGui )
    ON_COMMAND( 1003, OnScript )
    ON_COMMAND( 1004, OnLight )
    ON_COMMAND( 1005, OnSound )
    ON_COMMAND( 1006, OnAF )
    ON_COMMAND( 1007, OnParticle )
    ON_COMMAND( 1008, OnPDA )
    ON_COMMAND( 1009, OnAAS )
    ON_COMMAND( 1010, OnMaterial )
    ON_COMMAND( 1011, OnDECL )
END_MESSAGE_MAP()

void ToolsLauncherRun() {
    if ( !g_toolsLauncher ) {
        g_toolsLauncher = new CToolsLauncherWnd();
    }

    g_toolsLauncher->ShowWindow( SW_SHOW );
    g_toolsLauncher->UpdateWindow();
}
