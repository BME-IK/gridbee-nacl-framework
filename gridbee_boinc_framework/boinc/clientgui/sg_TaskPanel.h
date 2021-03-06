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

#ifndef __sg_TaskPanel__
#define __sg_TaskPanel__


#define SELECTBYRESULTNAME 0

#include "sg_PanelBase.h"


typedef struct {
    RESULT * result;
    char result_name[256];
    char project_url[256];
    int dotColor;
    wxArrayString slideShowFileNames;
    int lastSlideShown;
    double project_files_downloaded_time;
} TaskSelectionData;



///////////////////////////////////////////////////////////////////////////

class CSlideShowPanel : public wxPanel 
{
    DECLARE_DYNAMIC_CLASS( CSlideShowPanel )
    DECLARE_EVENT_TABLE()

	public:
        CSlideShowPanel();
		CSlideShowPanel( wxWindow* parent);
		~CSlideShowPanel();

        void OnSlideShowTimer(wxTimerEvent& WXUNUSED(event));
        void AdvanceSlideShow(bool changeSlide);
        void OnPaint(wxPaintEvent& WXUNUSED(event));
                
    private:
        wxTimer*                    m_ChangeSlideTimer;
        wxBitmap                    m_SlideBitmap;
        bool                        m_bCurrentSlideIsDefault;
        
};


///////////////////////////////////////////////////////////////////////////////
/// Class CSimpleTaskPanel
///////////////////////////////////////////////////////////////////////////////

#if 0
#ifdef __WXMAC__
#include "MacBitmapComboBox.h"
#else
#define CBOINCBitmapComboBox wxBitmapComboBox
#define EVT_BOINCBITMAPCOMBOBOX EVT_COMBOBOX
#endif
#endif

class CSimpleTaskPanel : public CSimplePanelBase 
{
    DECLARE_DYNAMIC_CLASS( CSimpleTaskPanel )
    DECLARE_EVENT_TABLE()

    public:
        CSimpleTaskPanel();
		CSimpleTaskPanel( wxWindow* parent);
		~CSimpleTaskPanel();

        TaskSelectionData* GetTaskSelectionData();
        wxString GetSelectedTaskString() { return m_TaskSelectionCtrl->GetValue(); }
        void Update(bool delayShow=false);
        wxRect GetProgressRect() { return m_ProgressRect; }
        void ReskinInterface();

	private:
        void OnTaskSelection(wxCommandEvent &event);
        void GetApplicationAndProjectNames(RESULT* result, wxString* appName, wxString* projName);
        wxString GetElapsedTimeString(double f);
        wxString GetTimeRemainingString(double f);
        wxString GetStatusString(RESULT* result);
        wxString FormatTime(float fBuffer);
        void FindSlideShowFiles(TaskSelectionData *selData);
        void UpdateTaskSelectionList();
        bool isRunning(RESULT* result);
		bool DownloadingResults();
		bool Suspended();
		bool ProjectUpdateScheduled();
		void DisplayIdleState();
        void OnPulseProgressIndicator(wxTimerEvent& event);

	protected:
#ifdef __WXMAC__
        void OnEraseBackground(wxEraseEvent& event);
#endif
        wxRect*                     m_progressBarRect;
		CTransparentStaticText*     			m_myTasksLabel;
		CBOINCBitmapComboBox*       m_TaskSelectionCtrl;
		CTransparentStaticText*     			m_TaskProjectLabel;
		CTransparentStaticText*     			m_TaskProjectName;
#if SELECTBYRESULTNAME
		CTransparentStaticText*     			m_TaskApplicationName;
#endif
        CSlideShowPanel*            m_SlideShowArea;
		CTransparentStaticText*     			m_ElapsedTimeValue;
		CTransparentStaticText*     			m_TimeRemainingValue;
		wxGauge*                    m_ProgressBar;
		CTransparentStaticText*     			m_ProgressValueText;
		CTransparentStaticText*     			m_StatusValueText;
		wxButton*                   m_TaskCommandsButton;
        wxRect                      m_ProgressRect;
        int                         m_oldWorkCount;
        int                         m_iPctDoneX10;
		time_t                      error_time;
        wxBitmap                    m_GreenDot;
        wxBitmap                    m_YellowDot;
        wxBitmap                    m_RedDot;
        wxTimer*                    m_pulseTimer;
        bool                        m_bStableTaskInfoChanged;
        int                         m_CurrentTaskSelection;
        wxString                    m_sNotAvailableString;
        wxString                    m_sNoProjectsString;
};

#endif //__sg_TaskPanel__
