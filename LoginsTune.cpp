#include <vcl.h>
#include <windows.h>
#pragma hdrstop
#pragma argsused
#include <mmsystem.h>
#include <inifiles.hpp>
#include <PluginAPI.h>
#include <IdHashMessageDigest.hpp>

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
	   PlaySound((PluginUserDir + "\\\\LoginsTune\\\\Online.wav").w_str(), NULL, SND_ASYNC | SND_FILENAME);
	}
	//OffLine
	else
	{
      if(FileExists(PluginUserDir + "\\\\LoginsTune\\\\Offline.wav"))
	   PlaySound((PluginUserDir + "\\\\LoginsTune\\\\Offline.wav").w_str(), NULL, SND_ASYNC | SND_FILENAME);
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

//Zapisywanie zasob�w
bool SaveResourceToFile(wchar_t* FileName, wchar_t* Res)
{
  HRSRC hrsrc = FindResource(HInstance, Res, RT_RCDATA);
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
  else
   return 0;
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
  //Wypakiwanie pliku Online.wav
  //357EDE8A246434C2D6DE9549B4FE6A19
  if(!FileExists(PluginUserDir + "\\\\LoginsTune\\\\Online.wav"))
   SaveResourceToFile((PluginUserDir + "\\\\LoginsTune\\\\Online.wav").w_str(),L"ID_ONLINE");
  else if(MD5File(PluginUserDir + "\\\\LoginsTune\\\\Online.wav")!="357EDE8A246434C2D6DE9549B4FE6A19")
   SaveResourceToFile((PluginUserDir + "\\\\LoginsTune\\\\Online.wav").w_str(),L"ID_ONLINE");
  //Wypakiwanie pliku Offline.wav
  //F2921334AA507CC0ADB09766321F6CBE
  if(!FileExists(PluginUserDir + "\\\\LoginsTune\\\\Offline.wav"))
   SaveResourceToFile((PluginUserDir + "\\\\LoginsTune\\\\Offline.wav").w_str(),L"ID_OFFLINE");
  else if(MD5File(PluginUserDir + "\\\\LoginsTune\\\\Offline.wav")!="F2921334AA507CC0ADB09766321F6CBE")
   SaveResourceToFile((PluginUserDir + "\\\\LoginsTune\\\\Offline.wav").w_str(),L"ID_OFFLINE");
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

  return 0;
}
//---------------------------------------------------------------------------

//Informacje o wtyczce
extern "C" __declspec(dllexport) PPluginInfo __stdcall AQQPluginInfo(DWORD AQQVersion)
{
  PluginInfo.cbSize = sizeof(TPluginInfo);
  PluginInfo.ShortName = L"LoginsTune";
  PluginInfo.Version = PLUGIN_MAKE_VERSION(1,1,0,0);
  PluginInfo.Description = L"Wtyczka dodaje d�wi�k zalogowania si� na g��wne konto Jabber oraz d�wi�k wylogowania si� z niego.";
  PluginInfo.Author = L"Krzysztof Grochocki (Beherit)";
  PluginInfo.AuthorMail = L"kontakt@beherit.pl";
  PluginInfo.Copyright = L"Krzysztof Grochocki (Beherit)";
  PluginInfo.Homepage = L"http://beherit.pl";

  return &PluginInfo;
}
//---------------------------------------------------------------------------
