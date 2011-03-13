#include "DvdCore.h"

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

    while (SUCCEEDED(m_pIME->GetEvent(&lEvent, &lParam1, &lParam2, lTimeOut)))
    {
        DbgLog((LOG_TRACE, 5, TEXT("Event: %#x"), lEvent)) ;

        HRESULT hr;
        switch(lEvent)
        {
            case EC_DVD_CURRENT_HMSF_TIME:
            {
				printf("ts");
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
                break;

            case EC_DVD_NO_FP_PGC:
                hr = m_pIDvdC2->PlayTitle(1, DVD_CMD_FLAG_None, NULL);
                ASSERT(SUCCEEDED(hr));
                break;
		
			case EC_DVD_PLAYPERIOD_AUTOSTOP:
				printf("Output sentence");
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

