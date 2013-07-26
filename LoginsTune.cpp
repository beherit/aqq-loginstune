//---------------------------------------------------------------------------
#include <vcl.h>
#include <windows.h>
#include <mmsystem.h>
#include <inifiles.hpp>
#pragma hdrstop
#pragma argsused
#include "Aqq.h"
//---------------------------------------------------------------------------

int WINAPI DllEntryPoint(HINSTANCE hinst, unsigned long reason, void* lpReserved)
{
  return 1;
}
//---------------------------------------------------------------------------

//Struktury-glowne-----------------------------------------------------------
TPluginLink PluginLink;
TPluginInfo PluginInfo;
PPluginStateChange StateChange;
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
	  if(FileExists(PluginUserDir + "\\\\LoginsTune\\\\Online.wav"))
	   PlaySound((PluginUserDir + "\\\\LoginsTune\\\\Online.wav").t_str(), NULL, SND_ASYNC | SND_FILENAME);
	}
	//OffLine
	else
	{
      if(FileExists(PluginUserDir + "\\\\LoginsTune\\\\Offline.wav"))
	   PlaySound((PluginUserDir + "\\\\LoginsTune\\\\Offline.wav").t_str(), NULL, SND_ASYNC | SND_FILENAME);
    }
  }
}
//---------------------------------------------------------------------------

//Hook na polaczenie sieci przy starcie AQQ
int __stdcall OnSetLastState (WPARAM wParam, LPARAM lParam)
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
int __stdcall OnStateChange(WPARAM wParam, LPARAM lParam)
{
  //Pobranie informacji o stanie konta
  StateChange = (PPluginStateChange)lParam;
  //Pobranie nowego stanu konta
  int NewState = StateChange->NewState;
  //Pobranie starego stanu konta
  int OldState = StateChange->OldState;
  //Pobranie informacji o zalogowaniu sie
  bool Authorized = StateChange->Authorized;
  //OnLine - Connecting
  if((!OldState)&&(NewState)&&(!Authorized))
   NetworkConnecting = true;
  //OnLine - Connected
  if((NetworkConnecting)&&(Authorized)&&(NewState==OldState))
  {
    //Pobranie stanu glownego konta
	TPluginStateChange PluginStateChange;
	PluginLink.CallService(AQQ_FUNCTION_GETNETWORKSTATE,(WPARAM)(&PluginStateChange),0);
	//Pobranie nowego stanu glownego konta
	int cNewState = PluginStateChange.NewState;
	//Pobranie starego stanu glownego konta
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
bool SaveResourceToFile(char *FileName, char *res)
{
  HRSRC hrsrc = FindResource(HInstance, res, RT_RCDATA);
  if(!hrsrc) return false;
  DWORD size = SizeofResource(HInstance, hrsrc);
  HGLOBAL hglob = LoadResource(HInstance, hrsrc);
  LPVOID rdata = LockResource(hglob);
  HANDLE hFile = CreateFile(FileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  DWORD writ;
  WriteFile(hFile, rdata, size, &writ, NULL);
  CloseHandle(hFile);
  return true;
}
//---------------------------------------------------------------------------

extern "C" int __declspec(dllexport) __stdcall Load(PPluginLink Link)
{
  //Linkowanie wtyczki z komunikatorem
  PluginLink = *Link;
  ChkSoundEnabled();
  //Pobranie sciezki do katalogu prywatnego uzytkownika
  PluginUserDir = GetPluginUserDir();
  //Folder wtyczki
  if(!DirectoryExists(PluginUserDir + "\\\\LoginsTune"))
   CreateDir(PluginUserDir + "\\\\LoginsTune");
  //Wypakowanie dzwiekow
  if(!FileExists(PluginUserDir + "\\\\LoginsTune\\\\Online.wav"))
   SaveResourceToFile((PluginUserDir + "\\\\LoginsTune\\\\Online.wav").t_str(),"ID_ONLINE");
  if(!FileExists(PluginUserDir + "\\\\LoginsTune\\\\Offline.wav"))
   SaveResourceToFile((PluginUserDir + "\\\\LoginsTune\\\\Offline.wav").t_str(),"ID_OFFLINE");
  //Hook na polaczenie sieci przy starcie AQQ
  PluginLink.HookEvent(AQQ_SYSTEM_SETLASTSTATE,OnSetLastState);
  //Hook na zmiane stanu sieci
  PluginLink.HookEvent(AQQ_SYSTEM_STATECHANGE,OnStateChange);

  return 0;
}
//---------------------------------------------------------------------------

extern "C" int __declspec(dllexport) __stdcall Unload()
{
  //Wyladowanie wszystkich hookow
  PluginLink.UnhookEvent(OnSetLastState);
  PluginLink.UnhookEvent(OnStateChange);
  //Usuniecie wskaznikow do struktur
  delete StateChange;

  return 0;
}
//---------------------------------------------------------------------------

//Informacje o wtyczce
extern "C" __declspec(dllexport) PPluginInfo __stdcall AQQPluginInfo(DWORD AQQVersion)
{
  PluginInfo.cbSize = sizeof(TPluginInfo);
  PluginInfo.ShortName = L"LoginsTune";
  PluginInfo.Version = PLUGIN_MAKE_VERSION(1,0,1,0);
  PluginInfo.Description = L"Wtyczka dodaje dŸwiêk zalogowania siê na g³ówne konto Jabber oraz dŸwiêk wylogowania siê z niego.";
  PluginInfo.Author = L"Krzysztof Grochocki (Beherit)";
  PluginInfo.AuthorMail = L"kontakt@beherit.pl";
  PluginInfo.Copyright = L"Krzysztof Grochocki (Beherit)";
  PluginInfo.Homepage = L"http://beherit.pl";

  return &PluginInfo;
}
//---------------------------------------------------------------------------
