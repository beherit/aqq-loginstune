//---------------------------------------------------------------------------
// Copyright (C) 2012-2015 Krzysztof Grochocki
//
// This file is part of LoginsTune
//
// LoginsTune is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// LoginsTune is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with GNU Radio; see the file COPYING. If not, write to
// the Free Software Foundation, Inc., 51 Franklin Street,
// Boston, MA 02110-1301, USA.
//---------------------------------------------------------------------------

#include <vcl.h>
#include <windows.h>
#include <inifiles.hpp>
#include <IdHashMessageDigest.hpp>
#include <PluginAPI.h>
#pragma hdrstop

int WINAPI DllEntryPoint(HINSTANCE hinst, unsigned long reason, void* lpReserved)
{
	return 1;
}
//---------------------------------------------------------------------------

//Struktury-glowne-----------------------------------------------------------
TPluginLink PluginLink;
TPluginInfo PluginInfo;
//---------------------------------------------------------------------------
bool NetworkConnecting = false;
//Sciezka-do-katalogu-prywatnego-wtyczek-------------------------------------
UnicodeString PluginUserDir;
//---------------------------------------------------------------------------

//Pobieranie sciezki katalogu prywatnego wtyczek
UnicodeString GetPluginUserDir()
{
	return StringReplace((wchar_t*)PluginLink.CallService(AQQ_FUNCTION_GETPLUGINUSERDIR,0,0), "\\", "\\\\", TReplaceFlags() << rfReplaceAll);
}
//---------------------------------------------------------------------------

//Sprawdzanie czy dziwieki zostaly wlaczone
bool ChkSoundEnabled()
{
	TStrings* IniList = new TStringList();
	IniList->SetText((wchar_t*)PluginLink.CallService(AQQ_FUNCTION_FETCHSETUP,0,0));
	TMemIniFile *Settings = new TMemIniFile(ChangeFileExt(Application->ExeName, ".INI"));
	Settings->SetStrings(IniList);
	delete IniList;
	UnicodeString SoundOff = Settings->ReadString("Sound","SoundOff","0");
	delete Settings;
	return !StrToBool(SoundOff);
}
//---------------------------------------------------------------------------

//Odgrywanie dzwieku OnLine/OffLine
void PlayLoginSound(bool OnLine)
{
	//Jezeli dzwieki sa wlaczone
	if(ChkSoundEnabled())
	{
		//OnLine
		if(OnLine)
		{
			if(FileExists(PluginUserDir + "\\\\LoginsTune\\\\Online.mp3"))
				PluginLink.CallService(AQQ_SYSTEM_PLAYSOUND,(WPARAM)(PluginUserDir + "\\\\LoginsTune\\\\Online.mp3").w_str(),2);
		}
		//OffLine
		else
		{
			if(FileExists(PluginUserDir + "\\\\LoginsTune\\\\Offline.mp3"))
				PluginLink.CallService(AQQ_SYSTEM_PLAYSOUND,(WPARAM)(PluginUserDir + "\\\\LoginsTune\\\\Offline.mp3").w_str(),2);
			}
	}
}
//---------------------------------------------------------------------------

//Hook na polaczenie sieci przy starcie AQQ
INT_PTR __stdcall OnSetLastState(WPARAM wParam, LPARAM lParam)
{
	//Pobranie stanu glownego konta
	TPluginStateChange PluginStateChange;
	PluginLink.CallService(AQQ_FUNCTION_GETNETWORKSTATE,(WPARAM)(&PluginStateChange),0);
	//Pobranie nowego stanu glownego konta
	int cNewState = PluginStateChange.NewState;
	//OnLine - Connected
	if(cNewState) PlayLoginSound(true);

	return 0;
}
//---------------------------------------------------------------------------

//Hook na zmiane stanu sieci
INT_PTR __stdcall OnStateChange(WPARAM wParam, LPARAM lParam)
{
	//Pobranie informacji o stanie konta
	TPluginStateChange StateChange = *(PPluginStateChange)lParam;
	//Pobranie nowego stanu konta
	int NewState = StateChange.NewState;
	//Pobranie starego stanu konta
	int OldState = StateChange.OldState;
	//Pobranie informacji o zalogowaniu sie
	bool Authorized = StateChange.Authorized;
	//OnLine - Connecting
	if((NewState)&&(!Authorized))
		NetworkConnecting = true;
	//OnLine - Connected
	if((NetworkConnecting)&&(Authorized)&&(NewState==OldState))
	{
		//Pobranie stanu konta
		TPluginStateChange PluginStateChange;
		PluginLink.CallService(AQQ_FUNCTION_GETNETWORKSTATE,(WPARAM)(&PluginStateChange),StateChange.UserIdx);
		//Pobranie nowego stanu konta
		int cNewState = PluginStateChange.NewState;
		//Pobranie starego stanu konta
		int cOldState = PluginStateChange.OldState;
		//OnLine
		if((cNewState==cOldState)&&(cNewState==NewState)&&(cOldState==OldState))
		{
			PlayLoginSound(true);
			NetworkConnecting = false;
		}
		else
			NetworkConnecting = false;
	}
	else if((NetworkConnecting)&&(Authorized)&&(NewState!=OldState))
		NetworkConnecting = false;
	//OffLine
	if((!NewState)&&(OldState)&&(Authorized))
		PlayLoginSound(false);

	return 0;
}
//---------------------------------------------------------------------------

//Zapisywanie zasobów
void ExtractRes(wchar_t* FileName, wchar_t* ResName, wchar_t* ResType)
{
	TPluginTwoFlagParams PluginTwoFlagParams;
	PluginTwoFlagParams.cbSize = sizeof(TPluginTwoFlagParams);
	PluginTwoFlagParams.Param1 = ResName;
	PluginTwoFlagParams.Param2 = ResType;
	PluginTwoFlagParams.Flag1 = (int)HInstance;
	PluginLink.CallService(AQQ_FUNCTION_SAVERESOURCE,(WPARAM)&PluginTwoFlagParams,(LPARAM)FileName);
}
//---------------------------------------------------------------------------

//Obliczanie sumy kontrolnej pliku
UnicodeString MD5File(UnicodeString FileName)
{
	if(FileExists(FileName))
	{
		UnicodeString Result;
		TFileStream *fs;
		fs = new TFileStream(FileName, fmOpenRead | fmShareDenyWrite);
		try
		{
			TIdHashMessageDigest5 *idmd5= new TIdHashMessageDigest5();
			try
			{
				Result = idmd5->HashStreamAsHex(fs);
			}
			__finally
			{
				delete idmd5;
			}
		}
		__finally
		{
			delete fs;
		}
		return Result;
	}
	else return 0;
}
//---------------------------------------------------------------------------

extern "C" INT_PTR __declspec(dllexport) __stdcall Load(PPluginLink Link)
{
	//Linkowanie wtyczki z komunikatorem
	PluginLink = *Link;
	//Pobranie sciezki do katalogu prywatnego uzytkownika
	PluginUserDir = GetPluginUserDir();
	//Folder wtyczki
	if(!DirectoryExists(PluginUserDir + "\\\\LoginsTune"))
		CreateDir(PluginUserDir + "\\\\LoginsTune");
	//Wypakiwanie pliku Online.mp3
	//17A92B0E67518AF3BA0F82B54971390D
	if(!FileExists(PluginUserDir + "\\\\LoginsTune\\\\Online.mp3"))
		ExtractRes((PluginUserDir + "\\\\LoginsTune\\\\Online.mp3").w_str(),L"ONLINE",L"DATA");
	else if(MD5File(PluginUserDir + "\\\\LoginsTune\\\\Online.mp3")!="17A92B0E67518AF3BA0F82B54971390D")
		ExtractRes((PluginUserDir + "\\\\LoginsTune\\\\Online.mp3").w_str(),L"ONLINE",L"DATA");
	//Wypakiwanie pliku Offline.mp3
	//F6BFD843EFB45119FF60820F50FFC929
	if(!FileExists(PluginUserDir + "\\\\LoginsTune\\\\Offline.mp3"))
		ExtractRes((PluginUserDir + "\\\\LoginsTune\\\\Offline.mp3").w_str(),L"OFFLINE",L"DATA");
	else if(MD5File(PluginUserDir + "\\\\LoginsTune\\\\Offline.mp3")!="F6BFD843EFB45119FF60820F50FFC929")
		ExtractRes((PluginUserDir + "\\\\LoginsTune\\\\Offline.mp3").w_str(),L"OFFLINE",L"DATA");
	//Hook na polaczenie sieci przy starcie AQQ
	PluginLink.HookEvent(AQQ_SYSTEM_SETLASTSTATE,OnSetLastState);
	//Hook na zmiane stanu sieci
	PluginLink.HookEvent(AQQ_SYSTEM_STATECHANGE,OnStateChange);

	return 0;
}
//---------------------------------------------------------------------------

extern "C" INT_PTR __declspec(dllexport) __stdcall Unload()
{
	//Wyladowanie wszystkich hookow
	PluginLink.UnhookEvent(OnSetLastState);
	PluginLink.UnhookEvent(OnStateChange);

	return 0;
}
//---------------------------------------------------------------------------

//Informacje o wtyczce
extern "C" __declspec(dllexport) PPluginInfo __stdcall AQQPluginInfo(DWORD AQQVersion)
{
	PluginInfo.cbSize = sizeof(TPluginInfo);
	PluginInfo.ShortName = L"LoginsTune";
	PluginInfo.Version = PLUGIN_MAKE_VERSION(1,3,2,0);
	PluginInfo.Description = L"Dodaje dŸwiêk zalogowania siê na g³ówne konto Jabber oraz dŸwiêk wylogowania siê z niego.";
	PluginInfo.Author = L"Krzysztof Grochocki";
	PluginInfo.AuthorMail = L"kontakt@beherit.pl";
	PluginInfo.Copyright = L"Krzysztof Grochocki";
	PluginInfo.Homepage = L"http://beherit.pl";
	PluginInfo.Flag = 0;
	PluginInfo.ReplaceDefaultModule = 0;

	return &PluginInfo;
}
//---------------------------------------------------------------------------
