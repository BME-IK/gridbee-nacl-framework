// This file is part of BOINC.
// http://boinc.berkeley.edu
// Copyright (C) 2008 University of California
//
// BOINC is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// BOINC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with BOINC.  If not, see <http://www.gnu.org/licenses/>.

#define TESTBIGICONPOPUP 0

#include "stdwx.h"
#include "miofile.h"
#include "Events.h"
#include "BOINCGUIApp.h"
#include "SkinManager.h"
#include "MainDocument.h"
#include "sg_TaskCommandPopup.h"
#include "sg_TaskPanel.h"
#include "boinc_api.h"
#include "filesys.h"
#include "res/GreenDot16.xpm"
#include "res/YellowDot16.xpm"
#include "res/RedDot16.xpm"


enum { redDot, yellowDot, greenDot };


IMPLEMENT_DYNAMIC_CLASS(CSlideShowPanel, wxPanel)

BEGIN_EVENT_TABLE(CSlideShowPanel, wxPanel)
//	EVT_ERASE_BACKGROUND(CSlideShowPanel::OnEraseBackground)
	EVT_TIMER(ID_CHANGE_SLIDE_TIMER, CSlideShowPanel::OnSlideShowTimer)
    EVT_PAINT(CSlideShowPanel::OnPaint)
END_EVENT_TABLE()

#define SLIDESHOWBORDER 3

CSlideShowPanel::CSlideShowPanel() {
}


CSlideShowPanel::CSlideShowPanel( wxWindow* parent ) :
    wxPanel( parent, wxID_ANY, wxDefaultPosition, wxSize(290+(2*SLIDESHOWBORDER), 126+(2*SLIDESHOWBORDER)), wxSTATIC_BORDER )
{
    SetBackgroundColour(*wxBLACK);

    m_ChangeSlideTimer = new wxTimer(this, ID_CHANGE_SLIDE_TIMER);
	m_ChangeSlideTimer->Start(10000);
    
    m_SlideBitmap = wxNullBitmap;
    m_bCurrentSlideIsDefault = false;

#ifdef __WXMAC__
    // Tell accessibility aids to ignore this panel (but not its contents)
    HIObjectSetAccessibilityIgnored((HIObjectRef)GetHandle(), true);
#endif    
}

CSlideShowPanel::~CSlideShowPanel()
{
	if ( m_ChangeSlideTimer->IsRunning() ) {
		m_ChangeSlideTimer->Stop();
	}
	delete m_ChangeSlideTimer;
}


void CSlideShowPanel::OnSlideShowTimer(wxTimerEvent& WXUNUSED(event)) {
    AdvanceSlideShow(true);
}


void CSlideShowPanel::AdvanceSlideShow(bool changeSlide) {
	double xRatio, yRatio, ratio;
    TaskSelectionData* selData = ((CSimpleTaskPanel*)GetParent())->GetTaskSelectionData();
    if (selData == NULL) return;

    int numSlides = selData->slideShowFileNames.size();

    if (numSlides <= 0) {
        if (m_bCurrentSlideIsDefault) return;
        
        CSkinSimple* pSkinSimple = wxGetApp().GetSkinManager()->GetSimple();
        wxASSERT(pSkinSimple);
        wxASSERT(wxDynamicCast(pSkinSimple, CSkinSimple));

        m_SlideBitmap = *pSkinSimple->GetWorkunitAnimationImage()->GetBitmap();
        if (m_SlideBitmap.Ok()) {
            m_bCurrentSlideIsDefault = true;
        }
    } else {
        // TODO: Should we allow slide show to advance if task is not running?
        if (selData->dotColor == greenDot) {    // Advance only if running
            if (changeSlide) {
                if (++(selData->lastSlideShown) >= numSlides) {
                    selData->lastSlideShown = 0;
                }
            }
        }
        if (selData->lastSlideShown < 0) {
            selData->lastSlideShown = 0;
        }
        
        wxBitmap *bm = new wxBitmap();
        bm->LoadFile(selData->slideShowFileNames[selData->lastSlideShown], wxBITMAP_TYPE_ANY);
        m_SlideBitmap = *bm;
        delete bm;

        if (m_SlideBitmap.Ok()) {
            m_bCurrentSlideIsDefault = false;
        }
    }
    if (m_SlideBitmap.Ok()) {
		// Check to see if they need to be rescaled to fit in the window
		ratio = 1.0;
		xRatio = ((double) GetSize().GetWidth() - (2*SLIDESHOWBORDER))/((double) m_SlideBitmap.GetWidth());
		yRatio = ((double) GetSize().GetHeight() - (2*SLIDESHOWBORDER))/((double) m_SlideBitmap.GetHeight());
		if ( xRatio < ratio ) {
			ratio = xRatio;
		}
		if ( yRatio < ratio ) {
			ratio = yRatio;
		}
		if ( ratio < 1.0 ) {
			wxImage img = m_SlideBitmap.ConvertToImage();
			img.Rescale((int) (m_SlideBitmap.GetWidth()*ratio), (int) (m_SlideBitmap.GetHeight()*ratio));
            wxBitmap *bm = new wxBitmap(img);
			m_SlideBitmap = *bm;
            delete bm;
		}
        Refresh();
    }
}


void CSlideShowPanel::OnPaint(wxPaintEvent& WXUNUSED(event)) 
{ 
        wxPaintDC dc(this);
        int w, h;
        wxPen oldPen = dc.GetPen();
        wxBrush oldBrush = dc.GetBrush();
        int oldMode = dc.GetBackgroundMode();
        wxPen bgPen(*wxBLUE, 3);
        dc.SetBackgroundMode(wxSOLID);
        dc.SetPen(bgPen);
        dc.SetBrush(*wxBLACK_BRUSH);
        
        GetSize(&w, &h);
        dc.DrawRectangle(0, 0, w, h);
        // Restore Mode, Pen and Brush 
        dc.SetBackgroundMode(oldMode);
        dc.SetPen(oldPen);
        dc.SetBrush(oldBrush);
        
        if(m_SlideBitmap.Ok()) 
        { 
            dc.DrawBitmap(m_SlideBitmap, 
                            (w - m_SlideBitmap.GetWidth())/2, 
                            (h - m_SlideBitmap.GetHeight())/2
                            ); 
        } 
} 

        



IMPLEMENT_DYNAMIC_CLASS(CSimpleTaskPanel, CSimplePanelBase)

BEGIN_EVENT_TABLE(CSimpleTaskPanel, CSimplePanelBase)
    EVT_BOINCBITMAPCOMBOBOX(ID_SGTASKSELECTOR, CSimpleTaskPanel::OnTaskSelection)
//    EVT_BUTTON(ID_TASKSCOMMANDBUTTON, CSimpleTaskPanel::OnTasksCommand)
//    EVT_LEFT_DOWN(CSimpleTaskPanel::OnTasksCommand)
    EVT_TIMER(ID_SIMPLE_PROGRESSPULSETIMER, CSimpleTaskPanel::OnPulseProgressIndicator)
#ifdef __WXMAC__
    EVT_ERASE_BACKGROUND(CSimpleTaskPanel::OnEraseBackground)    
#endif
END_EVENT_TABLE()

#if TESTBIGICONPOPUP
static wxString tempArray[] = {_T("String1"), _T("String2"), _T("String3") };
#endif

CSimpleTaskPanel::CSimpleTaskPanel() {
}

CSimpleTaskPanel::CSimpleTaskPanel( wxWindow* parent ) :
    CSimplePanelBase( parent )
{
    wxSize sz;
    int w, h;
    wxString str = wxEmptyString;

	m_pulseTimer = new wxTimer(this, ID_SIMPLE_PROGRESSPULSETIMER);
    m_oldWorkCount = -1;
    error_time = 0;
    m_GreenDot = wxBitmap((const char**)GreenDot16_xpm);
    m_YellowDot = wxBitmap((const char**)YellowDot16_xpm);
    m_RedDot = wxBitmap((const char**)RedDot16_xpm);
    m_GotBGBitMap = false; // Can't be made until parent has been laid out.
    m_bStableTaskInfoChanged = false;
    m_CurrentTaskSelection = -1;
    m_sNoProjectsString = _("You don't have any projects.  Please Add a Project.");
    m_sNotAvailableString = _("Not available");
    m_progressBarRect = NULL;

    SetFont(wxFont(SMALL_FONT,wxSWISS,wxNORMAL,wxNORMAL,false,wxT("Arial")));
	SetForegroundColour(*wxBLACK);
    
	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* bSizer2;
	bSizer2 = new wxBoxSizer( wxHORIZONTAL );
	
	m_myTasksLabel = new CTransparentStaticText( this, wxID_ANY, _("My Tasks:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_myTasksLabel->Wrap( -1 );
	bSizer2->Add( m_myTasksLabel, 0, wxRIGHT, 5 );
	
#if TESTBIGICONPOPUP
	m_TaskSelectionCtrl = new CBOINCBitmapComboBox( this, ID_SGTASKSELECTOR, wxT(""), wxDefaultPosition, wxDefaultSize, 3, tempArray, wxCB_READONLY ); 
    m_TaskSelectionCtrl->SetItemBitmap(0, m_RedDot);
    m_TaskSelectionCtrl->SetItemBitmap(1, m_GreenDot);
    m_TaskSelectionCtrl->SetItemBitmap(2, m_YellowDot);
//    m_TaskSelectionCtrl->SetStringSelection(tempArray[1]);
    m_TaskSelectionCtrl->SetSelection(1);
#else
	m_TaskSelectionCtrl = new CBOINCBitmapComboBox( this, ID_SGTASKSELECTOR, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY ); 
#endif
    // TODO: Might want better wording for Task Selection Combo Box tooltip
    str = _("Select a task to access");
    m_TaskSelectionCtrl->SetToolTip(str);
	bSizer2->Add( m_TaskSelectionCtrl, 1, wxRIGHT | wxEXPAND, SIDEMARGINS );
	
	bSizer1->Add( bSizer2, 0, wxEXPAND | wxTOP | wxLEFT, 10 );
	
    bSizer1->AddSpacer(5);
	
	wxBoxSizer* bSizer3;
	bSizer3 = new wxBoxSizer( wxHORIZONTAL );
	
	m_TaskProjectLabel = new CTransparentStaticText( this, wxID_ANY, _("From Project:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_TaskProjectLabel->Wrap( -1 );
	bSizer3->Add( m_TaskProjectLabel, 0, wxRIGHT, 5 );
	
	m_TaskProjectName = new CTransparentStaticText( this, wxID_ANY, wxT("SETI@home"), wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE );
	m_TaskProjectName->Wrap( -1 );
	m_TaskProjectName->SetFont(wxFont(LARGE_FONT,wxSWISS,wxNORMAL,wxNORMAL,false,wxT("Arial"))); 
	bSizer3->Add( m_TaskProjectName, 1, 0, 0 );
	
	bSizer1->Add( bSizer3, 0, wxLEFT | wxRIGHT | wxEXPAND, SIDEMARGINS );

#if SELECTBYRESULTNAME
	m_TaskApplicationName = new CTransparentStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE );
	m_TaskApplicationName->Wrap( -1 );

	bSizer1->Add( m_TaskApplicationName, 0, wxLEFT | wxRIGHT | wxEXPAND, SIDEMARGINS );
#endif

    bSizer1->AddSpacer(10);
	
    m_SlideShowArea = new CSlideShowPanel(this);
    m_SlideShowArea->SetMinSize(wxSize(290+(2*SLIDESHOWBORDER), 126+(2*SLIDESHOWBORDER)));
	m_SlideShowArea->Enable( false );
    
	bSizer1->Add( m_SlideShowArea, 0, wxLEFT | wxRIGHT | wxEXPAND, SIDEMARGINS );

    bSizer1->AddSpacer(10);
	
	m_ElapsedTimeValue = new CTransparentStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE );
	m_ElapsedTimeValue->Wrap( -1 );
	bSizer1->Add( m_ElapsedTimeValue, 0, wxLEFT | wxRIGHT | wxEXPAND, SIDEMARGINS );
	
    bSizer1->AddSpacer(7);
	
	m_TimeRemainingValue = new CTransparentStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE );
	m_TimeRemainingValue->Wrap( -1 );
	bSizer1->Add( m_TimeRemainingValue, 0, wxLEFT | wxRIGHT | wxEXPAND, SIDEMARGINS );
	
    bSizer1->AddSpacer(7);
	
	wxBoxSizer* bSizer4;
	bSizer4 = new wxBoxSizer( wxHORIZONTAL );
	
    // TODO: Standard Mac progress indicator's animation uses lots of CPU 
    // time, and also triggers unnecessary Erase events.  Should we use a 
    // non-standard progress indicator on Mac?  See also optimizations in 
    // CSimpleGUIPanel::OnEraseBackground and CSimpleTaskPanel::OnEraseBackground.
	m_ProgressBar = new wxGauge( this, wxID_ANY, 100, wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL );
    m_iPctDoneX10 = 1000;
	m_ProgressBar->SetValue( 100 );
    GetTextExtent(wxT("0"), &w, &h);
    m_ProgressBar->SetMinSize(wxSize(245, h));
    m_ProgressBar->SetToolTip(_("This task's progress"));
	bSizer4->Add( m_ProgressBar, 0, wxRIGHT, 5 );
	
	m_ProgressValueText = new CTransparentStaticText( this, wxID_ANY, wxT("100.0%"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT | wxST_NO_AUTORESIZE );
	m_ProgressValueText->Wrap( -1 );
	bSizer4->Add( m_ProgressValueText, 0, wxALIGN_RIGHT, 0 );
	
	bSizer1->Add( bSizer4, 0, wxLEFT | wxRIGHT | wxEXPAND, SIDEMARGINS );
	
    bSizer1->AddSpacer(7);
	
    // TODO: Can we determine the longest status string and initialize with it?
	m_StatusValueText = new CTransparentStaticText( this, wxID_ANY, m_sNoProjectsString, wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE );
//	m_StatusValueText = new CTransparentStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE );
	m_StatusValueText->Wrap( -1 );
	bSizer1->Add( m_StatusValueText, 0, wxLEFT | wxRIGHT | wxEXPAND, SIDEMARGINS );

    bSizer1->AddSpacer(7);
	
	m_TaskCommandsButton = new CSimpleTaskPopupButton( this, ID_TASKSCOMMANDBUTTON, _("Task Commands"), wxDefaultPosition, wxDefaultSize, 0 );
    m_TaskCommandsButton->SetToolTip(_("Pop up a menu of commands to apply to this task"));
	bSizer1->Add( m_TaskCommandsButton, 0, wxLEFT | wxRIGHT | wxEXPAND | wxALIGN_CENTER_HORIZONTAL, SIDEMARGINS );
    
    bSizer1->AddSpacer(10);
	
	this->SetSizer( bSizer1 );
	this->Layout();
    
    m_ProgressRect = m_ProgressBar->GetRect();
    m_ProgressRect.Inflate(1, 0);
}


CSimpleTaskPanel::~CSimpleTaskPanel()
{
    TaskSelectionData *selData;
    int count = m_TaskSelectionCtrl->GetCount();
	for(int j = count-1; j >=0; --j) {
        selData = (TaskSelectionData*)m_TaskSelectionCtrl->GetClientData(j);
        selData->slideShowFileNames.Clear();
        delete selData;
        m_TaskSelectionCtrl->SetClientData(j, NULL);
	}
    m_TaskSelectionCtrl->Clear();
    
    m_pulseTimer->Stop(); 
    delete m_pulseTimer;
    
    if (m_progressBarRect) {
        delete m_progressBarRect;
    }
}


void CSimpleTaskPanel::OnTaskSelection(wxCommandEvent& /*event*/)
{
    int sel = m_TaskSelectionCtrl->GetSelection();
    if (sel != m_CurrentTaskSelection) {
        m_CurrentTaskSelection = sel;
        m_bStableTaskInfoChanged = true;
        Update();
    }
}


void CSimpleTaskPanel::Update(bool delayShow) {
    wxString s = wxEmptyString;
    wxString projName = wxEmptyString;
    TaskSelectionData *selData;
    CMainDocument*      pDoc = wxGetApp().GetDocument();
    wxASSERT(pDoc);
    int                 workCount = pDoc->GetSimpleGUIWorkCount();

    // Workaround for Linux refresh problem
    static bool		wasDelayed = false;

#ifndef __WXMAC__
    Freeze();
#endif
    
    if ((workCount <= 0) || delayShow) {
        if ((workCount != m_oldWorkCount) || delayShow) {
	    wasDelayed = true;
            m_myTasksLabel->Hide();
            m_TaskSelectionCtrl->Hide();
            m_TaskProjectLabel->Hide();
            m_TaskProjectName->Hide();
#if SELECTBYRESULTNAME
            m_TaskApplicationName->Hide();
#endif
            m_SlideShowArea->Hide();
            m_ElapsedTimeValue->Hide();
            m_TimeRemainingValue->Hide();
            m_ProgressValueText->Hide();
            m_TaskCommandsButton->Hide();
            if (m_iPctDoneX10 >= 0) {
                m_iPctDoneX10 = -1;
                m_ProgressBar->Pulse();
                m_pulseTimer->Start(100);
            }
            this->Layout();
        }
        
        DisplayIdleState();
        
    } else {
        if ((m_oldWorkCount == 0) || wasDelayed) {
	    wasDelayed = false;
            m_myTasksLabel->Show();
            m_TaskSelectionCtrl->Show();
            m_TaskProjectLabel->Show();
            m_TaskProjectName->Show();
#if SELECTBYRESULTNAME
            m_TaskApplicationName->Show();
#endif
            m_SlideShowArea->Show();
            m_ElapsedTimeValue->Show();
            m_TimeRemainingValue->Show();
            m_ProgressValueText->Show();
            m_TaskCommandsButton->Show();
            this->Layout();
        }

        UpdateTaskSelectionList();
        
        // We now have valid result pointers, so extract our data
        int count = m_TaskSelectionCtrl->GetCount();
        if (count <= 0) {
            m_CurrentTaskSelection = -1;
        } else {
            if ((m_CurrentTaskSelection < 0) || (m_CurrentTaskSelection > count -1)) {
                m_TaskSelectionCtrl->SetSelection(0);
                m_CurrentTaskSelection = 0;
                m_bStableTaskInfoChanged = true;
            }
            selData = (TaskSelectionData*)m_TaskSelectionCtrl->GetClientData(m_CurrentTaskSelection);
            RESULT* result = selData->result;
            if (result) {
                if (m_bStableTaskInfoChanged) {
#if SELECTBYRESULTNAME
                    wxString str = wxEmptyString;
                    GetApplicationAndProjectNames(result, &s, &projName);
                    str.Printf(_("Application: %s"), s.c_str());
                    UpdateStaticText(&m_TaskApplicationName, str);
                    UpdateStaticText(&m_TaskProjectName, projName);
#else
                    GetApplicationAndProjectNames(result, NULL, &projName);
#endif
                    UpdateStaticText(&m_TaskProjectName, projName);
                    m_SlideShowArea->AdvanceSlideShow(false);
                    m_bStableTaskInfoChanged = false;
                }
                float f = result->elapsed_time;
                if (f == 0.) f = result->current_cpu_time;
//                f = result->final_elapsed_time;
                UpdateStaticText(&m_ElapsedTimeValue, GetElapsedTimeString(f));
                UpdateStaticText(&m_TimeRemainingValue, GetTimeRemainingString(result->estimated_cpu_time_remaining));
                int pctDoneX10 = result->fraction_done * 1000.0;
                if (m_iPctDoneX10 != pctDoneX10) {
                    if (m_iPctDoneX10 < 0) {
                        m_pulseTimer->Stop();
                    }
                    int pctDone = pctDoneX10 / 10;
                    if (pctDone != (m_iPctDoneX10 / 10)) {
                        m_ProgressBar->SetValue(pctDone);
                    }
                    s.Printf(_("%d.%d%%"), pctDoneX10 / 10, pctDoneX10 % 10 );
                    m_iPctDoneX10 = pctDoneX10;
                    UpdateStaticText(&m_ProgressValueText, s);
                }
                UpdateStaticText(&m_StatusValueText, GetStatusString(result));
            } else {
                UpdateStaticText(&m_TaskProjectName, m_sNotAvailableString);
#if SELECTBYRESULTNAME
                UpdateStaticText(&m_TaskApplicationName, _("Application: Not available") );
#endif
                UpdateStaticText(&m_ElapsedTimeValue, GetElapsedTimeString(-1.0));
                UpdateStaticText(&m_TimeRemainingValue, GetTimeRemainingString(-1.0));
                if (m_iPctDoneX10 >= 0) {
                    m_iPctDoneX10 = -1;
                    m_ProgressBar->Pulse();
                    m_pulseTimer->Start(100);
                }
                UpdateStaticText(&m_ProgressValueText, wxEmptyString);
                UpdateStaticText(&m_StatusValueText, GetStatusString(NULL));
            }
        }
    }
    m_oldWorkCount = workCount;

#ifndef __WXMAC__
    Thaw();
#endif
}


void CSimpleTaskPanel::ReskinInterface() {
    CSimplePanelBase::ReskinInterface();
    m_SlideShowArea->AdvanceSlideShow(false);
}


void CSimpleTaskPanel::OnPulseProgressIndicator(wxTimerEvent& /*event*/) {
    m_ProgressBar->Pulse();
}


TaskSelectionData* CSimpleTaskPanel::GetTaskSelectionData() {
    int count = m_TaskSelectionCtrl->GetCount();
    if (count <= 0) {
        return NULL;
    }

    int n = m_TaskSelectionCtrl->GetSelection();
    return (TaskSelectionData*)m_TaskSelectionCtrl->GetClientData(n);
}


// Either appName argument or projName argument may be NULL
void CSimpleTaskPanel::GetApplicationAndProjectNames(RESULT* result, wxString* appName, wxString* projName) {
    CMainDocument* pDoc = wxGetApp().GetDocument();
    RESULT*        state_result = NULL;
    wxString       strAppBuffer = wxEmptyString;
    wxString       strClassBuffer = wxEmptyString;

    wxASSERT(pDoc);
    wxASSERT(wxDynamicCast(pDoc, CMainDocument));

    state_result = pDoc->state.lookup_result(result->project_url, result->name);
    if (!state_result) {
        pDoc->ForceCacheUpdate();
        state_result = pDoc->state.lookup_result(result->project_url, result->name);
    }

    if (!state_result) return;
    
    if (appName != NULL) {
        WORKUNIT* wup = state_result->wup;
        if (!wup) return;
        APP* app = wup->app;
        if (!app) return;
        APP_VERSION* avp = state_result->avp;
        if (!avp) return;

        if (strlen(app->user_friendly_name)) {
            strAppBuffer = wxString(state_result->app->user_friendly_name, wxConvUTF8);
        } else {
            strAppBuffer = wxString(state_result->avp->app_name, wxConvUTF8);
        }
        
        if (strlen(avp->plan_class)) {
            strClassBuffer.Printf(
                wxT(" (%s)"),
                wxString(avp->plan_class, wxConvUTF8).c_str()
            );
        }

#if SELECTBYRESULTNAME
        appName->Printf(
            wxT("%d: %s%s %d.%02d %s"),
            state_result->slot,
            state_result->project->anonymous_platform?_("Local: "):wxT(""),
            strAppBuffer.c_str(),
            state_result->avp->version_num / 100,
            state_result->avp->version_num % 100,
            strClassBuffer.c_str()
        );
#else
        appName->Printf(wxT("%s%s %d.%02d %s (in slot %d)"),
            state_result->project->anonymous_platform?_("Local: "):wxT(""),
            strAppBuffer.c_str(),
            state_result->avp->version_num / 100,
            state_result->avp->version_num % 100,
            strClassBuffer.c_str(),
            state_result->slot
        );
#endif
    }
    
    if (projName != NULL) {
        *projName = wxString(state_result->project->project_name.c_str(), wxConvUTF8 );
        if (projName->IsEmpty()) {
            *projName = _("Not Available");
        }
    }
}


wxString CSimpleTaskPanel::GetElapsedTimeString(double f) {
    wxString s = wxEmptyString;
    wxString str = wxEmptyString;
    
    if (f < 0) {
        s = m_sNotAvailableString;
    } else {
        s = FormatTime(f);
    }
    str.Printf(_("Elapsed Time: %s"), s.c_str());
    return str;
}


wxString CSimpleTaskPanel::GetTimeRemainingString(double f) {
    wxString s = wxEmptyString;
    wxString str = wxEmptyString;
    
    if (f < 0) {
        s = m_sNotAvailableString;
    } else {
        s = FormatTime(f);
    }
    str.Printf(_("Time Remaining: %s"), s.c_str());
    return str;
}


wxString CSimpleTaskPanel::GetStatusString(RESULT* result) {
    wxString s = wxEmptyString;
    wxString str = wxEmptyString;
    
    if (result == NULL) {
        s = m_sNotAvailableString;
    } else {
        s = result_description(result, false);
    }
    
    str.Printf(_("Status: %s"), s.c_str());
    return str;
}


wxString CSimpleTaskPanel::FormatTime(float fBuffer) {
    wxInt32        iHour = 0;
    wxInt32        iMin = 0;
    wxInt32        iSec = 0;
    wxTimeSpan     ts;
	wxString strBuffer= wxEmptyString;

    if (0 >= fBuffer) {
        strBuffer = wxT("---");
    } else {
        iHour = (wxInt32)(fBuffer / (60 * 60));
        iMin  = (wxInt32)(fBuffer / 60) % 60;
        iSec  = (wxInt32)(fBuffer) % 60;

        ts = wxTimeSpan(iHour, iMin, iSec);

        strBuffer = ts.Format();
    }

    return strBuffer;
}


void CSimpleTaskPanel::FindSlideShowFiles(TaskSelectionData *selData) {
	RESULT* state_result;
    char urlDirectory[1024];
	char fileName[1024];
	char resolvedFileName[1024];
    int j;
    CMainDocument*      pDoc = wxGetApp().GetDocument();
    wxASSERT(pDoc);
    

    selData->slideShowFileNames.Clear();
    state_result = pDoc->state.lookup_result(selData->result->project_url, selData->result->name);
    if (!state_result) {
        pDoc->ForceCacheUpdate();
        state_result = pDoc->state.lookup_result(selData->result->project_url, selData->result->name);
    }
    if (state_result) {
        url_to_project_dir(state_result->project->master_url, urlDirectory);
        for(j=0; j<99; ++j) {
            sprintf(fileName, "%s/slideshow_%s_%02d", urlDirectory, state_result->app->name, j);
            if(boinc_resolve_filename(fileName, resolvedFileName, sizeof(resolvedFileName)) == 0) {
                if (boinc_file_exists(resolvedFileName)) {
                    selData->slideShowFileNames.Add(wxString(resolvedFileName,wxConvUTF8));
                }
            } else {
                break;
            }
        }

        if ( selData->slideShowFileNames.size() == 0 ) {
            for(j=0; j<99; ++j) {
                sprintf(fileName, "%s/slideshow_%02d", urlDirectory, j);
                if(boinc_resolve_filename(fileName, resolvedFileName, sizeof(resolvedFileName)) == 0) {
                    if (boinc_file_exists(resolvedFileName)) {
                        selData->slideShowFileNames.Add(wxString(resolvedFileName,wxConvUTF8));
                    }
                } else {
                    break;
                }
            }
        }
    }
    selData->lastSlideShown = -1;
}


void CSimpleTaskPanel::UpdateTaskSelectionList() {
    int i, j, count, newColor;
    TaskSelectionData *selData;
	RESULT* result;
	RESULT* ctrlResult;
    PROJECT* project;
    std::vector<bool>is_alive;
    CMainDocument*      pDoc = wxGetApp().GetDocument();
    wxASSERT(pDoc);
    
    count = m_TaskSelectionCtrl->GetCount();

	// Mark all inactive (this lets us loop only once)
    for (i=0; i<count; ++i) {
        is_alive.push_back(false);
    }
    
	// First update existing entries and add new ones
	for(int i = 0; i < (int) pDoc->results.results.size(); i++) {
		bool found = false;
        
		result = pDoc->result(i);
		// only check tasks that are active
		if ( result == NULL || !result->active_task ) {
			continue;
		}


		// loop through the items already in Task Selection Control to find this result
		for(j = 0; j < count; ++j) {
            selData = (TaskSelectionData*)m_TaskSelectionCtrl->GetClientData(j);
			if (!strcmp(result->name, selData->result_name) && 
                !strcmp(result->project_url, selData->project_url)
            ) {
                selData->result = result;
				found = true;
				is_alive.at(j) = true;
				break; // skip out of this loop
			}
		}
        
        // if it isn't currently one of the tabs then we have a new one!  lets add it
        if (!found) {
#if SELECTBYRESULTNAME
            wxString resname(result->name, wxConvUTF8);
#else
            wxString resname = wxEmptyString;
            GetApplicationAndProjectNames(result, &resname, NULL);
#endif
            selData = new TaskSelectionData;
            selData->result = result;
            strncpy(selData->result_name, result->name, sizeof(selData->result_name));
            strncpy(selData->project_url, result->project_url, sizeof(selData->project_url));
            selData->dotColor = -1;
            FindSlideShowFiles(selData);
            project = pDoc->state.lookup_project(result->project_url);
            if (project) {
                selData->project_files_downloaded_time = project->project_files_downloaded_time;
            } else {
                selData->project_files_downloaded_time = 0.0;
            }
            m_TaskSelectionCtrl->Append(resname, wxNullBitmap, (void*)selData);
        }
    }

    // Check items in descending order so deletion won't change indexes of items yet to be checked
	for(j = count-1; j >=0; --j) {
		if(! is_alive.at(j)) {
            selData = (TaskSelectionData*)m_TaskSelectionCtrl->GetClientData(j);
            selData->slideShowFileNames.Clear();
            delete selData;
			m_TaskSelectionCtrl->Delete(j);
            if (j == m_CurrentTaskSelection) {
                m_bStableTaskInfoChanged = true;
            }
		}
	}

    if ((m_CurrentTaskSelection >= 0) && !m_bStableTaskInfoChanged) {
        selData = (TaskSelectionData*)m_TaskSelectionCtrl->GetClientData(m_CurrentTaskSelection);
        project = pDoc->state.lookup_project(selData->project_url);
        if ( project && (project->project_files_downloaded_time > selData->project_files_downloaded_time) ) {
            FindSlideShowFiles(selData);
            selData->project_files_downloaded_time = project->project_files_downloaded_time;
        }
    }

    count = m_TaskSelectionCtrl->GetCount();
    for(j = 0; j < count; ++j) {
        selData = (TaskSelectionData*)m_TaskSelectionCtrl->GetClientData(j);
        ctrlResult = selData->result;
		if (Suspended() || (ctrlResult->suspended_via_gui)) {
            newColor = redDot;
        } else if (isRunning(ctrlResult)) {
            newColor = greenDot;
        } else if (ctrlResult->scheduler_state == CPU_SCHED_PREEMPTED) {
            newColor = yellowDot;
        } else {
            newColor = redDot;
        }

        if (newColor != selData->dotColor) {
            switch (newColor) {
            case greenDot:
            m_TaskSelectionCtrl->SetItemBitmap(j, m_GreenDot);
                break;
            case yellowDot:
            m_TaskSelectionCtrl->SetItemBitmap(j, m_YellowDot);
                break;
            case redDot:
            m_TaskSelectionCtrl->SetItemBitmap(j, m_RedDot);
                break;
            }
            selData->dotColor = newColor;
            m_TaskSelectionCtrl->Refresh();
        }
    }
}


bool CSimpleTaskPanel::isRunning(RESULT* result) {
	bool outcome = false;

	// It must be scheduled to be running
	if ( result->scheduler_state == CPU_SCHED_SCHEDULED ) {
		// If either the project or task have been suspended, then it cannot be running
		if ( !result->suspended_via_gui && !result->project_suspended_via_gui ) {
			CC_STATUS status;
            CMainDocument*      pDoc = wxGetApp().GetDocument();
            wxASSERT(pDoc);
            
			pDoc->GetCoreClientStatus(status);
			// Make sure that the core client isn't global suspended for some reason
			if ( status.task_suspend_reason == 0 || status.task_suspend_reason == SUSPEND_REASON_CPU_THROTTLE ) {
				outcome = true;
			}
		}
	}

	return outcome;
}


bool CSimpleTaskPanel::DownloadingResults() {
	bool return_value = false;
    CMainDocument*      pDoc = wxGetApp().GetDocument();
    wxASSERT(pDoc);
            
	if ( pDoc->results.results.size() > 0 ) {
		RESULT* result;
		for(unsigned int i=0; !return_value && i < pDoc->results.results.size(); i++ ) {
			result = pDoc->result(i);
			if ( result != NULL && result->state == RESULT_FILES_DOWNLOADING ) {
				return_value = true;
			}
		}
	}
	return return_value;
}

bool CSimpleTaskPanel::Suspended() {
	CC_STATUS status;
    CMainDocument*      pDoc = wxGetApp().GetDocument();
    wxASSERT(pDoc);
    
    bool result = false;
	pDoc->GetCoreClientStatus(status);
	if ( pDoc->IsConnected() && status.task_suspend_reason > 0 && status.task_suspend_reason != SUSPEND_REASON_DISK_SIZE &&  status.task_suspend_reason != SUSPEND_REASON_CPU_THROTTLE ) {
		result = true;
	}
	return result;
}

// Check to see if a project update is scheduled or in progress
bool CSimpleTaskPanel::ProjectUpdateScheduled() {
	PROJECT* project;
    CMainDocument*      pDoc = wxGetApp().GetDocument();
    wxASSERT(pDoc);
    
    int prjCount = pDoc->GetSimpleProjectCount();
	for(int i=0; i<prjCount; i++) {
		project = pDoc->state.projects[i];
		if ( project->sched_rpc_pending || project->master_url_fetch_pending || project->scheduler_rpc_in_progress ) {
			return true;
		}
	}
	return false;
}

void CSimpleTaskPanel::DisplayIdleState() {
    CMainDocument*      pDoc = wxGetApp().GetDocument();
    wxASSERT(pDoc);
            
    if ( pDoc->IsReconnecting() ) {
		error_time = 0;
        UpdateStaticText(&m_StatusValueText, _("Retrieving current status."));
	} else if ( pDoc->IsConnected() && pDoc->state.projects.size() == 0) {
		error_time = 0;
		UpdateStaticText(&m_StatusValueText, m_sNoProjectsString);
	} else if ( DownloadingResults() ) {
		error_time = 0;
		UpdateStaticText(&m_StatusValueText, _("Downloading work from the server."));
	} else if ( Suspended() ) {
		CC_STATUS status;
		pDoc->GetCoreClientStatus(status);
		if ( status.task_suspend_reason & SUSPEND_REASON_BATTERIES ) {
			UpdateStaticText(&m_StatusValueText, _("Processing Suspended:  Running On Batteries."));
		} else if ( status.task_suspend_reason & SUSPEND_REASON_USER_ACTIVE ) {
			UpdateStaticText(&m_StatusValueText, _("Processing Suspended:  User Active."));
		} else if ( status.task_suspend_reason & SUSPEND_REASON_USER_REQ ) {
			UpdateStaticText(&m_StatusValueText, _("Processing Suspended:  User paused processing."));
		} else if ( status.task_suspend_reason & SUSPEND_REASON_TIME_OF_DAY ) {
			UpdateStaticText(&m_StatusValueText, _("Processing Suspended:  Time of Day."));
		} else if ( status.task_suspend_reason & SUSPEND_REASON_BENCHMARKS ) {
			UpdateStaticText(&m_StatusValueText, _("Processing Suspended:  Benchmarks Running."));
		} else {
			UpdateStaticText(&m_StatusValueText, _("Processing Suspended."));
		}
	} else if ( ProjectUpdateScheduled() ) {
		error_time = 0;
		UpdateStaticText(&m_StatusValueText, _("Waiting to contact project servers."));
	} else {
		if ( error_time == 0 ) {
			error_time = time(NULL) + 10;
			UpdateStaticText(&m_StatusValueText, _("Retrieving current status"));
		} else if ( error_time < time(NULL) ) {
            // TODO: should we display "ERROR" like old Simple GUI?
			if ( pDoc->IsConnected() ) {
				UpdateStaticText(&m_StatusValueText, _("No work available to process"));
			} else {
				UpdateStaticText(&m_StatusValueText, _("Unable to connect to the core client"));
			}
		} else {
			UpdateStaticText(&m_StatusValueText, _("Retrieving current status"));
		}
	}
}


#ifdef __WXMAC__
// Avoid unnecessary drawing due to Mac progress indicator's animation
void CSimpleTaskPanel::OnEraseBackground(wxEraseEvent& event) {
    wxRect clipRect;
    wxDC *dc = event.GetDC();
     
    if (m_progressBarRect == NULL) {
        m_progressBarRect = new wxRect(m_ProgressBar->GetRect());
        m_progressBarRect->Inflate(1, 0);
    }
    dc->GetClippingBox(&clipRect.x, &clipRect.y, &clipRect.width, &clipRect.height);
    if (clipRect.IsEmpty() || m_progressBarRect->Contains(clipRect)) {
        return;
    }

    CSimplePanelBase::OnEraseBackground(event);
}
#endif
