#include "DvdCore.h"
#include "dvdsample.h"

#include <stdio.h>

void XTrace0(LPCTSTR lpszText)
{
   ::OutputDebugString(lpszText);
}

void XTrace(LPCTSTR lpszFormat, ...)
{
    va_list args;
    va_start(args, lpszFormat);
    int nBuf;
    TCHAR szBuffer[512]; // get rid of this hard-coded buffer
    nBuf = _vsntprintf(szBuffer, 511, lpszFormat, args);
    ::OutputDebugString(szBuffer);
    va_end(args);
}

#ifdef _DEBUG
#define XTRACE XTrace
#else
#define XTRACE
#endif

// OutputDebugString
//------------------------------------------------------------------------------
// Name: CDvdCore::OnDvdEvent()
// Desc: This method receives DVD messages from our WndProc and acts on them
//------------------------------------------------------------------------------
LRESULT CDvdCore::OnDvdEvent(UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    DbgLog((LOG_TRACE, 5, TEXT("CDvdCore::OnDvdEvent()"))) ;

    long lEvent;
	LONG_PTR lParam1, lParam2;
    long lTimeOut = 0;
//				OutputDebugString(L"Event!\n");

					DVD_HMSF_TIMECODE start;
					start.bHours = 0;
					start.bMinutes = 10;
					start.bSeconds = 0;
					start.bFrames = 20;
					DVD_HMSF_TIMECODE end;
					end.bHours = 0;
					end.bMinutes = 10;
					end.bSeconds = 05;
					end.bFrames = 20;



    while (SUCCEEDED(m_pIME->GetEvent(&lEvent, &lParam1, &lParam2, lTimeOut)))
    {
        DbgLog((LOG_TRACE, 5, TEXT("Got Event: %#x"), lEvent)) ;
		m_pCallback->UpdateStatus();
        HRESULT hr;
        switch(lEvent)
        {
            case EC_DVD_CURRENT_HMSF_TIME:
            {
				//  "This event is triggered at the beginning of every VOBU, which occurs every 0.4 to 1.0 seconds."
				OutputDebugString(L"pts event..");
                DVD_HMSF_TIMECODE * pTC = reinterpret_cast<DVD_HMSF_TIMECODE *>(&lParam1);
                m_CurTime = *pTC;
                m_pCallback->UpdateStatus(); // inform our client that something changed
            }
            break;

            case EC_DVD_CHAPTER_START:
                m_ulCurChapter = static_cast<ULONG>(lParam1);
                break;

            case EC_DVD_TITLE_CHANGE:
                m_ulCurTitle = static_cast<ULONG>(lParam1);
				XTRACE(L"title change event to %d ", m_ulCurTitle);
				if(m_ulCurTitle == 1) {

					//m_pIDvdC2->PlayPeriodInTitleAutoStop(1, &start, &end, DVD_CMD_FLAG_None, NULL);
				}
				XTRACE(L"NOT Commanded it to change though we are within title 1...\n");
                break;

            case EC_DVD_NO_FP_PGC:
                hr = m_pIDvdC2->PlayTitle(1, DVD_CMD_FLAG_None, NULL);
                ASSERT(SUCCEEDED(hr));
                break;
		
			case EC_DVD_PLAYPERIOD_AUTOSTOP:
			    playbackCount++;
				XTRACE(L"Got good autostop message! %d\n", playbackCount);
				m_eState = Nav_Paused;
				// probably wants something like "seek to that end now and run " [like it has totally lost current location...it does not pause at the end...it sets itself to something else...]

				if( (playbackCount % 2) == 0 || playbackCount > 2) {
					// restart...
					// m_pIDvdC2->PlayPeriodInTitleAutoStop(1, &start, &end, DVD_CMD_FLAG_None, NULL);
				}
				break;

            case EC_DVD_DOMAIN_CHANGE:
                switch (lParam1)
                {
                    case DVD_DOMAIN_FirstPlay:  // = 1
                    case DVD_DOMAIN_Stop:       // = 5
                        break ;

                    case DVD_DOMAIN_VideoManagerMenu:  // = 2
                    case DVD_DOMAIN_VideoTitleSetMenu: // = 3
                        // Inform the app to update the menu option to show "Resume" now
                        DbgLog((LOG_TRACE, 3, TEXT("DVD Event: Domain changed to VMGM(2)/VTSM(3) (%ld)"),
							static_cast<ULONG>(lParam1))) ;
                        m_bMenuOn = true;  // now menu is "On"
                        break ;

                    case DVD_DOMAIN_Title:      // = 4
                        // Inform the app to update the menu option to show "Menu" again
                        DbgLog((LOG_TRACE, 3, TEXT("DVD Event: Title Domain (%ld)"),
							static_cast<ULONG>(lParam1))) ;
                        m_bMenuOn = false ; // we are no longer in a menu
                        break ;
                } // end of domain change switch
                break;


            case EC_DVD_PARENTAL_LEVEL_CHANGE:
                {
                    TCHAR szMsg[1000];
                    hr = StringCchPrintf( szMsg, NUMELMS(szMsg),TEXT("Accept Parental Level Change Request to level %ld?"),
						static_cast<ULONG>(lParam1) );
                    if (IDYES == MessageBox(m_hWnd, szMsg,  TEXT("Accept Change?"), MB_YESNO))
                    {
                        m_pIDvdC2->AcceptParentalLevelChange(TRUE);
                    }
                    else
                    {
                        m_pIDvdC2->AcceptParentalLevelChange(FALSE);
                    }
                    break;
                }

            case EC_DVD_ERROR:
                DbgLog((LOG_TRACE, 3, TEXT("DVD Event: Error event received (code %ld)"),
					static_cast<ULONG>(lParam1))) ;
                switch (lParam1)
                {
                case DVD_ERROR_Unexpected:
                    MessageBox(m_hWnd,
                        TEXT("An unexpected error (possibly incorrectly authored content)")
                        TEXT("\nwas encountered.")
                        TEXT("\nCan't playback this DVD-Video disc."),
                        TEXT("Error"), MB_OK | MB_ICONINFORMATION) ;
                    m_pIMC->Stop() ;
                    break ;

                case DVD_ERROR_CopyProtectFail:
                    MessageBox(m_hWnd,
                        TEXT("Key exchange for DVD copy protection failed.")
                        TEXT("\nCan't playback this DVD-Video disc."),
                        TEXT("Error"), MB_OK | MB_ICONINFORMATION) ;
                    m_pIMC->Stop() ;
                    break ;

                case DVD_ERROR_InvalidDVD1_0Disc:
                    MessageBox(m_hWnd,
                        TEXT("This DVD-Video disc is incorrectly authored for v1.0  of the spec.")
                        TEXT("\nCan't playback this disc."),
                        TEXT("Error"), MB_OK | MB_ICONINFORMATION) ;
                    m_pIMC->Stop() ;
                    break ;

                case DVD_ERROR_InvalidDiscRegion:
                    MessageBox(m_hWnd,
                        TEXT("This DVD-Video disc cannot be played, because it is not")
                        TEXT("\nauthored to play in the current system region.")
                        TEXT("\nThe region mismatch may be fixed by changing the")
                        TEXT("\nsystem region (with DVDRgn.exe)."),
                        TEXT("Error"), MB_OK | MB_ICONINFORMATION) ;
                    m_pIMC->Stop() ;
                    ChangeDvdRegion();
                    break ;

                case DVD_ERROR_LowParentalLevel:
                    MessageBox(m_hWnd,
                        TEXT("Player parental level is set lower than the lowest parental")
                        TEXT("\nlevel available in this DVD-Video content.")
                        TEXT("\nCannot playback this DVD-Video disc."),
                        TEXT("Error"), MB_OK | MB_ICONINFORMATION) ;
                    m_pIMC->Stop() ;
                    break ;

                case DVD_ERROR_MacrovisionFail:
                    MessageBox(m_hWnd,
                        TEXT("This DVD-Video content is protected by Macrovision.")
                        TEXT("\nThe system does not satisfy Macrovision requirement.")
                        TEXT("\nCan't continue playing this disc."),
                        TEXT("Error"), MB_OK | MB_ICONINFORMATION) ;
                    m_pIMC->Stop() ;
                    break ;

                case DVD_ERROR_IncompatibleSystemAndDecoderRegions:
                    MessageBox(m_hWnd,
                        TEXT("No DVD-Video disc can be played on this system, because ")
                        TEXT("\nthe system region does not match the decoder region.")
                        TEXT("\nPlease contact the manufacturer of this system."),
                        TEXT("Error"), MB_OK | MB_ICONINFORMATION) ;
                    m_pIMC->Stop() ;
                    break ;

                case DVD_ERROR_IncompatibleDiscAndDecoderRegions:
                    MessageBox(m_hWnd,
                        TEXT("This DVD-Video disc cannot be played on this system, because it is")
                        TEXT("\nnot authored to be played in the installed decoder's region."),
                        TEXT("Error"), MB_OK | MB_ICONINFORMATION) ;
                    m_pIMC->Stop() ;
                    break ;
                }  // end of switch (lParam1)
                break ;

            // Next is warning
            case EC_DVD_WARNING:
                switch (lParam1)
                {
                case DVD_WARNING_InvalidDVD1_0Disc:
                    DbgLog((LOG_ERROR, 1, TEXT("DVD Warning: Current disc is not v1.0 spec compliant"))) ;
                    break ;

                case DVD_WARNING_FormatNotSupported:
                    DbgLog((LOG_ERROR, 1, TEXT("DVD Warning: The decoder does not support the new format."))) ;
                    break ;

                case DVD_WARNING_IllegalNavCommand:
                    DbgLog((LOG_ERROR, 1, TEXT("DVD Warning: An illegal navigation command was encountered."))) ;
                    break ;

                case DVD_WARNING_Open:
                    DbgLog((LOG_ERROR, 1, TEXT("DVD Warning: Could not open a file on the DVD disc."))) ;
                    MessageBox(m_hWnd,
                        TEXT("A file on the DVD disc could not be opened. Playback may not continue."),
                        TEXT("Warning"), MB_OK | MB_ICONINFORMATION) ;
                    break ;

                case DVD_WARNING_Seek:
                    DbgLog((LOG_ERROR, 1, TEXT("DVD Warning: Could not seek in a file on the DVD disc."))) ;
                    MessageBox(m_hWnd,
                        TEXT("Could not move to a different part of a file on the DVD disc. Playback may not continue."),
                        TEXT("Warning"), MB_OK | MB_ICONINFORMATION) ;
                    break ;

                case DVD_WARNING_Read:
                    DbgLog((LOG_ERROR, 1, TEXT("DVD Warning: Could not read a file on the DVD disc."))) ;
                    MessageBox(m_hWnd,
                        TEXT("Could not read part of a file on the DVD disc. Playback may not continue."),
                        TEXT("Warning"), MB_OK | MB_ICONINFORMATION) ;
                    break ;

                default:
                    DbgLog((LOG_ERROR, 1, TEXT("DVD Warning: An unknown (%ld) warning received."),
						static_cast<ULONG>(lParam1))) ;
                    break ;
                }
                break ;

            case EC_DVD_BUTTON_CHANGE:
               break;

            case EC_DVD_STILL_ON:
                if (TRUE == lParam1) // if there is a still without buttons, we can call StillOff
                    m_bStillOn = true;
                break;

            case EC_DVD_STILL_OFF:
                m_bStillOn = false; // we are no longer in a still
                break;

        } // end of switch(lEvent)

        m_pIME->FreeEventParams(lEvent, lParam1, lParam2) ;

    } // end of while(GetEvent())

    return 0;
}

//------------------------------------------------------------------------------
// Name: CApp::DrawStatus()
// Desc: This method draws our status test (time, title, chapter) on the screen
//------------------------------------------------------------------------------

void CApp::DrawStatus(HDC hDC) 
{
    DbgLog((LOG_TRACE, 5, TEXT("CApp::DrawStatus()"))) ;

    TCHAR location[35];
    HRESULT hr = StringCchPrintf(location, NUMELMS(location), TEXT("Title: %-6uChapter: %u\0"), m_pDvdCore->GetTitle(), 
        m_pDvdCore->GetChapter());
    TextOut(hDC, 10, 50, location, lstrlen(location));

    TCHAR time[250];

	float time_per_fps = 1.0/23.97; // a good guess! see http://betterlogic.com/roger/2011/10/idvdinfo2-fps-framerate

	float current_seconds = m_pDvdCore->GetTime().bHours*3600+m_pDvdCore->GetTime().bMinutes*60+m_pDvdCore->GetTime().bSeconds+(time_per_fps*m_pDvdCore->GetTime().bFrames);

	// bring it "up" to its more arguably accurate time elapsed (mplayer) time
	float correct_seconds = current_seconds * 30/29.97;
	int hours = correct_seconds/3600;
	int minutes = correct_seconds/3600/60;
	float corrected_just_seconds = correct_seconds - (hours*3600) - (minutes*60);

    hr = StringCchPrintf(time, NUMELMS(time), TEXT("29.97 fps time: %02d:%02d:%4.02f\0"), hours, 
        minutes, corrected_just_seconds);
    TextOut(hDC, 10, 65, time, lstrlen(time));

	hr = StringCchPrintf(time, NUMELMS(time), TEXT("30 fps Time: %02d:%02d:%02d f%d\0"), m_pDvdCore->GetTime().bHours,
    m_pDvdCore->GetTime().bMinutes, m_pDvdCore->GetTime().bSeconds, m_pDvdCore->GetTime().bFrames);
    TextOut(hDC, 10, 80, time, lstrlen(time));

    if(timeGetTime() <= (m_dwProhibitedTime + 5000)) // if less than 5 seconds has passed [huh?]
    {
        SetTextColor(hDC, RGB(255, 0, 0));
        TextOut(hDC, 180, 80, TEXT("Prohibited!"), 11);
    }
}

