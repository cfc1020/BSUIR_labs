#include <windef.h>
#include <tchar.h>
#include <windows.h>
//#include <io.h>

#include "app.h"
#include "DB.h"
#include "db_wrapper.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE _hPrevInst,
                   PSTR _lpCmdLine, int nCmdShow)
{
  //_setmode( _fileno( stdin ), _O_BINARY );
  //_setmode( _fileno( stdout ), _O_BINARY );

  try {
    DB_Wrapper db_w = DB_Wrapper(TEXT("DB.dll"));
    App(hInst).run(nCmdShow);

    Abonent *ab = create_abonent(0, NULL, NULL, NULL, TEXT("va"), NULL, NULL, NULL, NULL);
    DWORD *ids = (DWORD*) malloc(sizeof(DWORD) * 10);
    DWORD count = db_w.find_abonents(ids, 10, ab);
    for ( int i = 0; i < (count > 10 ? 10 : count); ++i )
    {
      Abonent* tmp_ab = create_abonent();
      db_w.get_by_id(ids[i], tmp_ab);
      _tprintf(TEXT("%d %s %s %s %s %s %s %s %s\n"), tmp_ab->id, tmp_ab->phone_no, tmp_ab->family_name, tmp_ab->name,
                                      tmp_ab->middle_name, tmp_ab->street, tmp_ab->house,
                                      tmp_ab->building, tmp_ab->flat);
      clear_abonent(tmp_ab);
    }
    clear_abonent(ab);

  } catch (...) {
    throw;
  }

  return 0;
}
